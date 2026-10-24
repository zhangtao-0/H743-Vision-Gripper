/* H743 2MiB dual-bank backend: only reserved Bank2 sectors4..7 may be written.
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
static void NvInvalidate(uint32_t address,uint32_t bytes){
#if defined(SCB_CCR_DC_Msk)
 if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)address,(int32_t)((bytes+31U)&~31U));
#else
 (void)address;(void)bytes;
#endif
}
int BoardNv_Read(BoardNvRegion region,void *out,uint32_t bytes){
 if(!out||!NvValid(region,bytes))return 0;
 NvInvalidate(nv_slot[region].address,bytes);
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
 NvInvalidate(nv_slot[region].address,bytes);
 if(result==HAL_OK&&memcmp(data,(const void *)nv_slot[region].address,bytes))result=HAL_ERROR;
 nv_busy=0;return result==HAL_OK;
}
