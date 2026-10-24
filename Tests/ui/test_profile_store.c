#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform_hal.h"
#include "ui_profile_store.h"
#include "integration_config.h"
HostFlashRegs host_flash_regs;HostScb host_scb;
static unsigned erases,programs,cut;
void SCB_InvalidateDCache_by_Addr(uint32_t *p,int32_t size){(void)p;(void)size;}
HAL_StatusTypeDef HAL_FLASH_Unlock(void){return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void){return HAL_OK;}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *error){
 assert(e->Banks==FLASH_BANK_2&&e->NbSectors==1&&(e->Sector==4||e->Sector==5));
 *error=0;erases++;memset((void*)(uintptr_t)(e->Sector==4?BOARD_PROFILE_FLASH_A:BOARD_PROFILE_FLASH_B),255,0x20000);return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint32_t source){
 assert(type==FLASH_TYPEPROGRAM_FLASHWORD&&address>=BOARD_PROFILE_FLASH_A&&address<BOARD_PROFILE_FLASH_B+0x20000&&!(address&31U));programs++;
 if(cut&&programs==cut){memcpy((void*)(uintptr_t)address,(void*)(uintptr_t)source,12);return HAL_ERROR;}
 memcpy((void*)(uintptr_t)address,(void*)(uintptr_t)source,32);return HAL_OK;
}
int main(void){
 unsigned i;UiProfile p,out,second;union{float f;uint32_t bits;} cpr;
 void *memory=VirtualAlloc((void*)0x08180000,0x80000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(memory==(void*)0x08180000);
 memset(memory,255,0x80000);memset((void*)0x081C0000,0xA5,0x40000);
 assert(!UiProfileStore_Load(&out));memset(&p,0,sizeof(p));p.flags=1;cpr.f=1061.268f;p.cpr_bits=cpr.bits;
 assert(UiProfileStore_Save(&p)&&p.sequence==1&&UiProfileStore_Load(&out));assert(erases==1&&programs==5);
 for(i=1;i<=5;i++){
  cut=i;programs=0;p.motor_target=50;assert(!UiProfileStore_Save(&p));assert(UiProfileStore_Load(&out)&&out.sequence==1);
 }
 cut=0;programs=0;p.motor_target=100;assert(UiProfileStore_Save(&p)&&p.sequence==2);second=p;
 ((unsigned char*)BOARD_PROFILE_FLASH_B)[20]^=1;assert(UiProfileStore_Load(&out)&&out.sequence==1);memcpy((void*)BOARD_PROFILE_FLASH_B,&second,sizeof(second));
 memset(&p,0,sizeof(p));assert(UiProfileStore_Save(&p)&&p.sequence==3&&!p.flags);assert(UiProfileStore_Load(&out)&&!out.flags);
 host_flash_regs.OPTSR_CUR=FLASH_OPTSR_SWAP_BANK_OPT;i=erases;assert(!UiProfileStore_Save(&p)&&erases==i);
 for(i=0;i<0x40000;i++)assert(((unsigned char*)0x081C0000)[i]==0xA5);
 VirtualFree(memory,0,MEM_RELEASE);puts("PASS H7 store model: actual sectors4/5 code, all five partial-write positions, CRC fallback, tombstone, bank-swap guard; limits6/7 untouched");return 0;
}
