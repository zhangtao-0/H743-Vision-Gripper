#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_settings.h"
#include "ui_profile_store.h"
#include "motor_service.h"
#include "feetech_sts_bus.h"
#include "feetech_sts_control.h"
static uint32_t view_flags;
uint32_t UiPort_ViewFlags(void){return view_flags;}
void UiPort_ApplyViewFlags(uint32_t f){view_flags=f;}
static UiProfile slots[2];static MotorStatus motor;static FeetechStsFeedback_t feedback;
static int32_t params[21];static unsigned writes,applies;static int online,busy,maintenance,break_write;
unsigned char App_MotionReady(void){return (unsigned char)online;}
uint8_t FeetechStsControl_IsMotionState(void){return (uint8_t)busy;}
const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void){return &feedback;}
uint8_t FeetechStsControl_GetParamInfo(FeetechStsParamId_t id,FeetechStsParamInfo_t *p){if(id<0||id>=21)return 0;p->value=params[id];p->min_value=id==12?20:0;p->max_value=10000;p->default_value=id==12?10:50;return 1;}
uint8_t FeetechStsControl_GetParamValue(FeetechStsParamId_t id,int32_t *v){*v=params[id];return 1;}
uint8_t FeetechStsControl_SetParamValue(FeetechStsParamId_t id,int32_t v){assert(id>=2&&id<=20);params[id]=v;applies++;return 1;}
void MotorService_Snapshot(MotorStatus *m){*m=motor;}
uint8_t MotorService_BeginMaintenance(void){if(maintenance||busy)return 0;maintenance=1;return 1;}
void MotorService_EndMaintenance(void){maintenance=0;}
uint8_t MotorService_ApplyParameters(float cpr,uint32_t rpm,uint32_t kp,uint32_t ki,uint32_t kd){motor.control.config.counts_per_rev=cpr;motor.control.target_rpm=(float)rpm;motor.control.config.kp=kp/1000.0f;motor.control.config.ki=ki/1000.0f;motor.control.config.kd=kd/1000.0f;return 1;}
int UiProfileStore_Load(UiProfile *out){int i=UiProfile_Latest(&slots[0],&slots[1]);if(i<0)return 0;*out=slots[i];return 1;}
int UiProfileStore_Save(UiProfile *value){
 int old=UiProfile_Latest(&slots[0],&slots[1]),target=old==0?1:0;
 UiProfile p=*value;p.magic=UI_PROFILE_MAGIC;p.version=UI_PROFILE_VERSION;p.sequence=old<0?1:slots[old].sequence+1;p.crc=UiProfile_Crc(&p);
 memset(&slots[target],255,sizeof(p));writes++;
 if(break_write){memcpy(&slots[target],&p,64);return 0;}
 slots[target]=p;*value=p;return 1;
}
static void Boot(void){unsigned i;online=0;for(i=0;i<21;i++)params[i]=50;params[0]=12;params[1]=180;params[12]=10;motor.control.config.max_rpm=400;motor.control.config.counts_per_rev=1061.268f;motor.control.target_rpm=0;motor.control.config.kp=1.8f;motor.control.config.ki=5;motor.control.config.kd=0;}
int main(void){
 memset(slots,255,sizeof(slots));Boot();UiSettings_Init();assert(!UiSettings_HasProfile()&&writes==0&&applies==0);
 view_flags=2U|(3U<<2)|(1U<<4);motor.control.target_rpm=95;assert(UiSettings_Save()==UI_ACCEPTED);assert(UiSettings_HasProfile()&&writes==1&&!maintenance);
 online=1;params[19]=1;params[4]=777;motor.control.config.kp=2.5f;assert(UiSettings_Save()==UI_ACCEPTED);assert(UiSettings_Sequence()==2);
 view_flags=0;Boot();UiSettings_Init();assert(view_flags==(2U|(3U<<2)|(1U<<4)));assert(writes==2&&UiSettings_HasProfile()&&motor.control.target_rpm==95&&motor.control.config.kp==2.5f&&motor.control.config.counts_per_rev>1061.26f&&applies==0);
 online=1;UiSettings_ApplySts();assert(params[4]==777&&params[0]==12&&params[1]==180&&params[19]==0&&applies==18); /* ID12 compiled 10 is left intact. */
 busy=1;assert(UiSettings_Save()==UI_BUSY&&UiSettings_Load()==UI_BUSY&&UiSettings_Clear()==UI_BUSY&&writes==2);busy=0;
 break_write=1;assert(UiSettings_Save()==UI_FAILED&&!maintenance&&UiSettings_Sequence()==2);break_write=0;
 assert(UiSettings_Load()==UI_ACCEPTED&&motor.control.target_rpm==95);
 assert(UiSettings_Clear()==UI_ACCEPTED&&!UiSettings_HasProfile());Boot();UiSettings_Init();assert(!UiSettings_HasProfile()&&motor.control.target_rpm==0);
 puts("PASS settings: no boot writes, save/reboot, exact float CPR, pending STS apply, mechanical endpoints excluded");
 puts("PASS settings: motion gates, interrupted-save recovery, atomic clear tombstone, no motor auto-enable path");return 0;
}
