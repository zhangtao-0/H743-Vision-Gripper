/* STM32H743ZI 2MiB: limit records in Bank2 sectors6/7; linker ROM ends 0x0817FFFF.
 * Boot profiles independently reserve sectors4/5.
 * Alternating 32-byte records keep the last valid configuration during a save.
 * No erase/program occurs during startup or Load. */
#include "claw_limit_storage.h"
#include "board_nv_storage.h"
#include <string.h>

typedef struct { uint32_t w[8]; } Record;
#define RECORD_MAGIC 0x374C4347UL
#define RECORD_VERSION 1U

static uint32_t RecordCrc(const Record *r)
{
    uint32_t crc = 0xFFFFFFFFU, i, j;
    const uint8_t *p = (const uint8_t *)r;
    for (i = 0; i < 28; i++) {
        crc ^= p[i];
        for (j = 0; j < 8; j++) crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xEDB88320U);
    }
    return ~crc;
}

static uint8_t Valid(const Record *r)
{
    return r->w[0] == RECORD_MAGIC && r->w[1] == RECORD_VERSION && r->w[7] == RecordCrc(r) &&
           (r->w[3] & 0xFFFFU) <= 4095 && (r->w[3] >> 16) <= 4095 &&
           (r->w[4] & 0xFFFFU) <= 1 && (r->w[4] >> 16) <= 1;
}

static int ReadRecords(Record records[2])
{return BoardNv_Read(BOARD_NV_LIMIT_A,&records[0],sizeof(Record))&&BoardNv_Read(BOARD_NV_LIMIT_B,&records[1],sizeof(Record));}
static const Record *Latest(Record records[2])
{
 const Record *a=&records[0],*b=&records[1];uint8_t va=Valid(a),vb=Valid(b);
 if(!va)return vb?b:0;if(!vb)return a;
 return (int32_t)(b->w[2]-a->w[2])>0?b:a;
}

HAL_StatusTypeDef ClawLimitStorage_Load(ClawLimitStorageData_t *out)
{
    Record records[2];const Record *r;
    if (!out || !ReadRecords(records)) return HAL_ERROR;
    r = Latest(records); if (!r) return HAL_ERROR;
    out->sequence = r->w[2];
    out->open_position = (uint16_t)r->w[3]; out->close_position = (uint16_t)(r->w[3] >> 16);
    out->limits_valid = (uint8_t)r->w[4]; out->sweep_range = (uint8_t)(r->w[4] >> 16);
    return HAL_OK;
}

HAL_StatusTypeDef ClawLimitStorage_SaveConfig(uint16_t open,uint16_t close,uint8_t valid,uint8_t sweep)
{
 Record records[2],verify;const Record *old;BoardNvRegion slot;
 Record r={{RECORD_MAGIC,RECORD_VERSION,1U,0,0,0xFFFFFFFFU,0xFFFFFFFFU,0}};
 if(open>4095||close>4095||valid>1||sweep>1||(valid&&open==close))return HAL_ERROR;
 if(!ReadRecords(records))return HAL_ERROR;
 old=Latest(records);slot=old==&records[0]?BOARD_NV_LIMIT_B:BOARD_NV_LIMIT_A;
 r.w[2]=old?old->w[2]+1U:1U;r.w[3]=open|((uint32_t)close<<16);r.w[4]=valid|((uint32_t)sweep<<16);r.w[7]=RecordCrc(&r);
 if(!BoardNv_Rewrite(slot,&r,sizeof(r))||!BoardNv_Read(slot,&verify,sizeof(verify))||!Valid(&verify)||memcmp(&r,&verify,sizeof(r)))return HAL_ERROR;
 return HAL_OK;
}

HAL_StatusTypeDef ClawLimitStorage_Save(uint16_t open, uint16_t close)
{ return ClawLimitStorage_SaveConfig(open, close, 1U, 0U); }
HAL_StatusTypeDef ClawLimitStorage_Clear(void)
{ return ClawLimitStorage_SaveConfig(0U, 0U, 0U, 0U); }
