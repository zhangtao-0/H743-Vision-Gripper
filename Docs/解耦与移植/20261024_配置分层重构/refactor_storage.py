from pathlib import Path
H=Path(__file__).resolve().parents[3];A=H/'project/App'
def put(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf8')
put(A/'platform/board_nv_storage.h','''#ifndef BOARD_NV_STORAGE_H
#define BOARD_NV_STORAGE_H
/* HAL-free, region-scoped NV interface. Backend owns erase/program/cache rules. */
#include <stdint.h>
typedef enum {BOARD_NV_PROFILE_A,BOARD_NV_PROFILE_B,BOARD_NV_LIMIT_A,BOARD_NV_LIMIT_B,BOARD_NV_COUNT} BoardNvRegion;
int BoardNv_Read(BoardNvRegion region,void *out,uint32_t bytes);
/* Rewrite one reserved region. Caller must select inactive slot and retain the old valid slot. */
int BoardNv_Rewrite(BoardNvRegion region,const void *data,uint32_t bytes);
#endif
''')
put(A/'platform/board_nv_storage.c','''#include "board_nv_storage.h"
#include "target_config.h"
#include PLATFORM_NV_BACKEND
''')
put(A/'platform/mcu/stm32h743/nv_backend.inc','''/* H743 2MiB dual-bank backend: only reserved Bank2 sectors4..7 may be written.
 * Keep startup/linker memory map in agreement; another chip replaces this backend. */
#include "board_config.h"
#include <string.h>
typedef struct {uint32_t address,bank,sector;} NvSlot;
static const NvSlot nv_slot[BOARD_NV_COUNT]={
 {BOARD_PROFILE_FLASH_A,BOARD_PROFILE_FLASH_BANK,BOARD_PROFILE_FLASH_SECTOR_A},
 {BOARD_PROFILE_FLASH_B,BOARD_PROFILE_FLASH_BANK,BOARD_PROFILE_FLASH_SECTOR_B},
 {BOARD_LIMIT_FLASH_A,BOARD_LIMIT_FLASH_BANK,BOARD_LIMIT_FLASH_SECTOR_A},
 {BOARD_LIMIT_FLASH_B,BOARD_LIMIT_FLASH_BANK,BOARD_LIMIT_FLASH_SECTOR_B}
};
static uint8_t nv_busy;
static int NvValid(BoardNvRegion region,uint32_t bytes){
 unsigned i,j;
 if((unsigned)region>=BOARD_NV_COUNT||!bytes||bytes>BOARD_FLASH_SECTOR_BYTES)return 0;
 for(i=0;i<BOARD_NV_COUNT;i++){
  const NvSlot *s=&nv_slot[i];
  if(s->bank!=FLASH_BANK_2||s->sector<4U||s->sector>7U||
   s->address!=0x08100000UL+s->sector*0x20000UL||BOARD_FLASH_SECTOR_BYTES!=0x20000UL)return 0;
  for(j=0;j<i;j++)if(s->address==nv_slot[j].address)return 0;
 }
 return 1;
}
int BoardNv_Read(BoardNvRegion region,void *out,uint32_t bytes){
 if(!out||!NvValid(region,bytes))return 0;
 if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)nv_slot[region].address,(int32_t)((bytes+31U)&~31U));
 memcpy(out,(const void *)nv_slot[region].address,bytes);return 1;
}
int BoardNv_Rewrite(BoardNvRegion region,const void *data,uint32_t bytes){
 FLASH_EraseInitTypeDef erase={0};HAL_StatusTypeDef result;uint32_t offset,error;
#pragma data_alignment=32
 static uint8_t word[32];
 if(!data||!NvValid(region,bytes)||bytes%32U||BOARD_FLASH_PROGRAM_UNIT!=32U||nv_busy||
  (FLASH->OPTSR_CUR&FLASH_OPTSR_SWAP_BANK_OPT))return 0;
 nv_busy=1;
 if(HAL_FLASH_Unlock()!=HAL_OK){nv_busy=0;return 0;}
 erase.TypeErase=FLASH_TYPEERASE_SECTORS;erase.Banks=nv_slot[region].bank;erase.Sector=nv_slot[region].sector;
 erase.NbSectors=1;erase.VoltageRange=FLASH_VOLTAGE_RANGE_3;
 result=HAL_FLASHEx_Erase(&erase,&error);
 for(offset=0;result==HAL_OK&&offset<bytes;offset+=32U){
  memcpy(word,(const uint8_t *)data+offset,32U);
  result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,nv_slot[region].address+offset,(uint32_t)word);
 }
 (void)HAL_FLASH_Lock();
 if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)nv_slot[region].address,(int32_t)bytes);
 if(result==HAL_OK&&memcmp(data,(const void *)nv_slot[region].address,bytes))result=HAL_ERROR;
 nv_busy=0;return result==HAL_OK;
}
''')
put(A/'modules/ui/ui_profile_store.c','''/* Portable two-slot record policy; chip erase/program/cache stays in BoardNv. */
#include "ui_profile_store.h"
#include "board_nv_storage.h"
#include <string.h>
static int Read(UiProfile records[2]){
 return BoardNv_Read(BOARD_NV_PROFILE_A,&records[0],sizeof(UiProfile))&&
        BoardNv_Read(BOARD_NV_PROFILE_B,&records[1],sizeof(UiProfile));
}
int UiProfileStore_Load(UiProfile *out){
 UiProfile records[2];int index;if(!out||!Read(records))return 0;
 index=UiProfile_Latest(&records[0],&records[1]);if(index<0)return 0;*out=records[index];return 1;
}
int UiProfileStore_Save(UiProfile *value){
 UiProfile records[2],write;int old;uint8_t slot;
 if(!value||!Read(records))return 0;
 old=UiProfile_Latest(&records[0],&records[1]);slot=(uint8_t)(old==0?1:0);
 write=*value;write.magic=UI_PROFILE_MAGIC;write.version=UI_PROFILE_VERSION;
 write.sequence=old<0?1U:records[old].sequence+1U;write.crc=UiProfile_Crc(&write);
 if(!UiProfile_Valid(&write)||!BoardNv_Rewrite(slot?BOARD_NV_PROFILE_B:BOARD_NV_PROFILE_A,&write,sizeof(write)))return 0;
 if(!Read(records)||!UiProfile_Valid(&records[slot])||memcmp(&write,&records[slot],sizeof(write)))return 0;
 *value=write;return 1;
}
''')
# Retain the legacy filename/API for project compatibility, but remove H7 register dependence.
p=A/'platform/claw_limit_storage_h7.c';s=p.read_text(encoding='utf8');s=s.replace('#include "integration_config.h"','#include "board_nv_storage.h"')
a=s.index('static const Record *Latest(void)');b=s.index('HAL_StatusTypeDef ClawLimitStorage_Load',a)
s=s[:a]+'''static const Record *Latest(Record records[2])
{
 const Record *a=&records[0],*b=&records[1];uint8_t va,vb;
 if(!BoardNv_Read(BOARD_NV_LIMIT_A,&records[0],sizeof(Record))||
    !BoardNv_Read(BOARD_NV_LIMIT_B,&records[1],sizeof(Record)))return 0;
 va=Valid(a);vb=Valid(b);if(!va)return vb?b:0;if(!vb)return a;
 return (int32_t)(b->w[2]-a->w[2])>0?b:a;
}

'''+s[b:]
s=s.replace('    const Record *r;','    Record records[2];const Record *r;',1).replace('r = Latest();','r = Latest(records);',1)
a=s.index('HAL_StatusTypeDef ClawLimitStorage_SaveConfig');b=s.index('HAL_StatusTypeDef ClawLimitStorage_Save(',a)
s=s[:a]+'''HAL_StatusTypeDef ClawLimitStorage_SaveConfig(uint16_t open,uint16_t close,uint8_t valid,uint8_t sweep)
{
 Record records[2],verify;const Record *old;BoardNvRegion slot;
 Record r={{RECORD_MAGIC,RECORD_VERSION,1U,0,0,0xFFFFFFFFU,0xFFFFFFFFU,0}};
 if(open>4095||close>4095||valid>1||sweep>1||(valid&&open==close))return HAL_ERROR;
 if(!BoardNv_Read(BOARD_NV_LIMIT_A,&records[0],sizeof(Record))||!BoardNv_Read(BOARD_NV_LIMIT_B,&records[1],sizeof(Record)))return HAL_ERROR;
 old=Latest(records);slot=old==&records[0]?BOARD_NV_LIMIT_B:BOARD_NV_LIMIT_A;
 r.w[2]=old?old->w[2]+1U:1U;r.w[3]=open|((uint32_t)close<<16);r.w[4]=valid|((uint32_t)sweep<<16);r.w[7]=RecordCrc(&r);
 if(!BoardNv_Rewrite(slot,&r,sizeof(r))||!BoardNv_Read(slot,&verify,sizeof(verify))||!Valid(&verify)||memcmp(&r,&verify,sizeof(r)))return HAL_ERROR;
 return HAL_OK;
}

'''+s[b:];put(p,s)
print('HAL-free NV interface and H743 backend created; record versions and addresses preserved')
