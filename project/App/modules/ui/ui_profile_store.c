/* Portable two-slot record policy; chip erase/program/cache stays in BoardNv. */
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
