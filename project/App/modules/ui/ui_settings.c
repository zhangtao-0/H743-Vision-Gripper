#include "ui_settings.h"
#include "ui_port.h"
#include "ui_profile_store.h"
#include "feetech_sts_control.h"
#include "feetech_sts_bus.h"
#include "motor_service.h"
#include "app_main.h"
#include <string.h>
static UiProfile saved;
static uint8_t available;
static uint8_t StsBusy(void){return (uint8_t)(App_MotionReady() && (FeetechStsControl_IsMotionState()||FeetechStsBus_GetFeedback()->moving));}
static uint8_t ValidateSettings(const UiProfile *p){
 uint8_t i;FeetechStsParamInfo_t info;MotorStatus m;
 if(!UiProfile_Valid(p)||!p->flags)return 0;
 MotorService_Snapshot(&m);if(p->motor_target>(uint32_t)m.control.config.max_rpm)return 0;
 for(i=2;i<21;i++){
  if(!FeetechStsControl_GetParamInfo((FeetechStsParamId_t)i,&info))return 0;
  if(p->sts[i]<info.min_value || p->sts[i]>info.max_value){
   /* Legacy compiled ID12 starts at 10 ms but runtime writes clamp to 20. */
   if(i!=12 || p->sts[i]!=info.default_value)return 0;
  }
 }
 return 1;
}
static uint8_t ApplyMotor(const UiProfile *p){union{uint32_t bits;float value;} cpr;cpr.bits=p->cpr_bits;return MotorService_ApplyParameters(cpr.value,p->motor_target,p->kp_milli,p->ki_milli,p->kd_milli);}
void UiSettings_Init(void){
 available=0;memset(&saved,0,sizeof(saved));
 if(UiProfileStore_Load(&saved) && ValidateSettings(&saved) && ApplyMotor(&saved)){available=1;UiPort_ApplyViewFlags(saved.reserved[0]==UI_PROFILE_VIEW_TAG?saved.reserved[1]:0);}
}
void UiSettings_ApplySts(void){
 uint8_t i;int32_t current;
 if(!available)return;
 for(i=2;i<21;i++){
  if(i==12 && saved.sts[i]==10 && FeetechStsControl_GetParamValue((FeetechStsParamId_t)i,&current) && current==10)continue;
  (void)FeetechStsControl_SetParamValue((FeetechStsParamId_t)i,i==19?0:saved.sts[i]);
 }
 /* IDs0/1 belong to separately saved mechanical limits; never overwritten here. */
}
UiResult UiSettings_Save(void){
 UiProfile p;MotorStatus m;FeetechStsParamInfo_t info;uint8_t i;int ok;union{float value;uint32_t bits;} cpr;
 if(StsBusy()||!MotorService_BeginMaintenance())return UI_BUSY;
 memset(&p,0,sizeof(p));p.flags=1;
 for(i=2;i<21;i++){
  if(!FeetechStsControl_GetParamInfo((FeetechStsParamId_t)i,&info)){MotorService_EndMaintenance();return UI_FAILED;}
  p.sts[i]=App_MotionReady()?info.value:(available?saved.sts[i]:info.default_value);
 }
 MotorService_Snapshot(&m);p.motor_target=(uint32_t)m.control.target_rpm;p.kp_milli=(uint32_t)(m.control.config.kp*1000+0.5f);
 p.ki_milli=(uint32_t)(m.control.config.ki*1000+0.5f);p.kd_milli=(uint32_t)(m.control.config.kd*1000+0.5f);cpr.value=m.control.config.counts_per_rev;p.cpr_bits=cpr.bits;
 p.reserved[0]=UI_PROFILE_VIEW_TAG;p.reserved[1]=UiPort_ViewFlags();
 p.magic=UI_PROFILE_MAGIC;p.version=UI_PROFILE_VERSION;p.crc=UiProfile_Crc(&p);
 if(!ValidateSettings(&p)){MotorService_EndMaintenance();return UI_BLOCKED;}
 ok=UiProfileStore_Save(&p);MotorService_EndMaintenance();
 if(!ok)return UI_FAILED;saved=p;available=1;return UI_ACCEPTED;
}
UiResult UiSettings_Load(void){
 UiProfile p;
 if(StsBusy()||!MotorService_BeginMaintenance())return UI_BUSY;
 if(!UiProfileStore_Load(&p)||!ValidateSettings(&p)){MotorService_EndMaintenance();return UI_OFFLINE;}
 if(!ApplyMotor(&p)){MotorService_EndMaintenance();return UI_FAILED;}
 saved=p;available=1;UiPort_ApplyViewFlags(p.reserved[0]==UI_PROFILE_VIEW_TAG?p.reserved[1]:0);if(App_MotionReady())UiSettings_ApplySts();MotorService_EndMaintenance();return UI_ACCEPTED;
}
UiResult UiSettings_Clear(void){
 UiProfile p;int ok;
 if(StsBusy()||!MotorService_BeginMaintenance())return UI_BUSY;
 memset(&p,0,sizeof(p));ok=UiProfileStore_Save(&p);MotorService_EndMaintenance();
 if(!ok)return UI_FAILED;saved=p;available=0;return UI_ACCEPTED;
}
uint8_t UiSettings_HasProfile(void){return available;}
uint32_t UiSettings_Sequence(void){return saved.sequence;}
