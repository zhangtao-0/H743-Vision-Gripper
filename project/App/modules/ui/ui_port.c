/* Target-only business binding. Navigation never parses UART strings. */
#include "ui_port.h"
#include "vision_link.h"
#include "ui_settings.h"
#include "ui_service.h"
#include "integration_console.h"
#include "app_main.h"
#include "feetech_sts_control.h"
#include "feetech_sts_bus.h"
#include "motor_service.h"
#include "motor_config.h"
#include "motor_plot.h"
#include "input_control.h"
#include "board_camera.h"
#include "board_storage.h"
#include "board_peripherals.h"
#include "integration_service.h"
#include "usb_console.h"
#include <stdio.h>
#include <string.h>
static const char *const params[]={"OPEN_deg","CLOSE_deg","GRIP_PROFILE","DEFAULT_SPEED","OPEN_SPEED","CLOSE_SPEED","SWEEP_SPEED","SWEEP_STEP","STEP_ms","TORQUE","HOLD_TORQUE","FEEDBACK_ms","IDLE_FB_ms","MIN_MOVE","SETTLE_ms","LOW_VOLT_mV","SLIP_deg","COMPENSATE","RELEASE_ALLOW","CLAW_PRINT","PRINT_ms","MOTOR_RPM","KP_milli","KI_milli","KD_milli"};
static const char *const commands[]={"Report status","Camera capture ON","Camera capture OFF","Motor STOP","Motor clear fault","Motor ENABLE","Motor reverse","PWM outputs OFF","STS stop request","STS open/release","STS close/grip","Still capture (RAM)","Image rotate +90","Image mirror L/R","Image flip U/D","LCD rotate +90","Edge preview toggle","Auto grasp (unavailable)","Quiet all streams","Live image resume"};
static const char *const defaults[]={"Restore STS RAM","Restore motor RAM","SAVE boot profile","LOAD boot profile","CLEAR boot profile"};
static int32_t Milli(float x){return (int32_t)(x*1000.0f+0.5f);}
static uint8_t StsReady(void){return (uint8_t)(App_MotionReady() && FeetechStsBus_GetFeedback()->online);}
static uint8_t Busy(void){return (uint8_t)(MotorService_IsRunning() || (App_MotionReady() && (FeetechStsControl_IsMotionState() || FeetechStsBus_GetFeedback()->moving)));}
static const char *ParamName(uint8_t id){return id<25?params[id]:"?";}
static const char *CommandName(uint8_t id){
 if(id==20)return Input_DebugEnabled(0)?"STS keys: ON":"STS keys: OFF";
 if(id==21)return Input_DebugEnabled(1)?"Motor keys: ON":"Motor keys: OFF";
 if(id==22)return MotorPlot_IsEnabled()?"PID plot ONLY: ON":"PID plot ONLY: OFF";
 if(id==23)return "Quiet all streams";
 return id<20?commands[id]:"?";
}
static const char *DefaultName(uint8_t id){return id<5?defaults[id]:"?";}
static UiResult ParamRead(uint8_t id,UiParameter *out){
 FeetechStsParamInfo_t info;MotorStatus m;
 memset(out,0,sizeof(*out));
 if(id<21){
  if(!FeetechStsControl_GetParamInfo((FeetechStsParamId_t)id,&info))return UI_FAILED;
  out->value=info.value;out->min=info.min_value;out->max=info.max_value;out->factory=info.default_value;out->step=id<2?0:1;return StsReady()?UI_ACCEPTED:UI_OFFLINE;
 }
 if(id>24)return UI_FAILED;
 MotorService_Snapshot(&m);if(!m.ready)return UI_OFFLINE;
 out->min=0;out->max=id==21?(int32_t)m.control.config.max_rpm:100000;out->step=id==21?5:50;
  if(id==21){out->value=(int32_t)m.control.target_rpm;out->factory=(int32_t)MOTOR_DEFAULT_RPM;}
 else if(id==22){out->value=Milli(m.control.config.kp);out->factory=Milli(MOTOR_KP);}
 else if(id==23){out->value=Milli(m.control.config.ki);out->factory=Milli(MOTOR_KI);}
 else {out->value=Milli(m.control.config.kd);out->factory=Milli(MOTOR_KD);}
 return UI_ACCEPTED;
}
static UiResult ParamWrite(uint8_t id,int32_t value,int32_t expected){
 UiParameter current;UiResult result;MotorStatus m;uint32_t saved;uint8_t ok;
 if(id<21){
  if(id<2)return UI_BLOCKED;
  if(!StsReady())return UI_OFFLINE;if(Busy())return UI_BUSY;
  result=ParamRead(id,&current);if(result!=UI_ACCEPTED)return result;
  if(current.value!=expected)return UI_STALE;
  if(value<current.min||value>current.max)return UI_BLOCKED;
  return FeetechStsControl_SetParamValue((FeetechStsParamId_t)id,value)?UI_ACCEPTED:UI_FAILED;
 }
 /* Atomically compare and set against the IRQ-owned EC11/enable state. */
 saved=__get_PRIMASK();__disable_irq();result=ParamRead(id,&current);
 if(result==UI_ACCEPTED && current.value!=expected)result=UI_STALE;
 if(result==UI_ACCEPTED && (value<current.min||value>current.max))result=UI_BLOCKED;
 if(result==UI_ACCEPTED){
  MotorService_Snapshot(&m);
  if(id==21)ok=MotorService_SetTarget((uint16_t)value);
  else if(id>=22&&id<=24)ok=MotorService_SetPid(id==22?(uint32_t)value:(uint32_t)Milli(m.control.config.kp),id==23?(uint32_t)value:(uint32_t)Milli(m.control.config.ki),id==24?(uint32_t)value:(uint32_t)Milli(m.control.config.kd));
  else ok=0;
  if(!ok)result=m.control.enabled?UI_BUSY:UI_BLOCKED;
 }
 __set_PRIMASK(saved);return result;
}
UiResult UiPort_RunCommand(uint8_t id,UiActionSource source){
 HAL_StatusTypeDef r=HAL_ERROR;
 if((unsigned)source>UI_ACTION_USB)return UI_FAILED;
 switch(id){
 case 20:case 21:return Input_DebugSet((uint8_t)(id-20U),!Input_DebugEnabled((uint8_t)(id-20U)))?UI_ACCEPTED:UI_BUSY;
 case 22:if(MotorPlot_IsEnabled())return MotorPlot_Enable(0)?UI_ACCEPTED:UI_BUSY;
  IntegrationConsole_Quiet();return MotorPlot_Enable(1)?UI_ACCEPTED:UI_BUSY;
 case 23:IntegrationConsole_Quiet();return UI_ACCEPTED;
 case 0:Integration_StatusReport();MotorService_Report();return UI_ACCEPTED;
 case 1:if(!BoardCamera_GetStatus()->ready)return UI_OFFLINE;BoardCamera_Enable(1);return UI_ACCEPTED;
 case 2:BoardCamera_Enable(0);return UI_ACCEPTED;
 case 3:MotorService_Stop();return UI_ACCEPTED;
 case 4:MotorService_ClearFault();return UI_ACCEPTED;
 case 5:return MotorService_Enable(1)?UI_ACCEPTED:UI_BLOCKED;
 case 6:MotorService_Reverse();return UI_ACCEPTED;
 case 7:BoardPwm_Stop();return UI_ACCEPTED;
 case 11:return BoardCamera_Snapshot()==HAL_OK?UI_ACCEPTED:UI_OFFLINE;
 case 12:case 13:case 14:{const BoardCameraStatus *c=BoardCamera_GetStatus();
  if(!c->ready)return UI_OFFLINE;
  BoardCamera_SetTransform(id==12?c->rotation+1U:c->rotation,id==13?!c->mirror:c->mirror,id==14?!c->flip:c->flip);return UI_ACCEPTED;}
 case 15:UiService_SetRotation(UiService_Rotation()+1U);return UI_ACCEPTED;
 case 16:if(!BoardCamera_GetStatus()->ready)return UI_OFFLINE;BoardCamera_SetEdges(!BoardCamera_GetStatus()->edges);return UI_ACCEPTED;
 case 17:return UI_UNAVAILABLE; /* Requires calibrated arm, vision target and task planning. */
 case 18:IntegrationConsole_Quiet();return UI_ACCEPTED;
 case 19:if(!BoardCamera_GetStatus()->ready)return UI_OFFLINE;BoardCamera_Resume();return UI_ACCEPTED;
 case 8:case 9:case 10:
  if(!StsReady())return UI_OFFLINE;
  if(id!=8 && (!FeetechStsControl_IsMotionRangeReady()||FeetechStsControl_IsLimitSetting()))return UI_BLOCKED;
  if(id==8)r=FeetechStsControl_StopMotion();
  else if(id==9)r=FeetechStsControl_ConfirmRelease();
  else r=FeetechStsControl_StartGrip();
  if(r==HAL_OK)FeetechStsControl_SetActionSource(source==UI_ACTION_SCREEN?FEETECH_STS_OWNER_SCREEN:source==UI_ACTION_HOST?FEETECH_STS_OWNER_VOFA:FEETECH_STS_OWNER_USB_DIRECT);
  return r==HAL_OK?UI_ACCEPTED:UI_BLOCKED;
 default:return UI_FAILED;
 }
}
static UiResult CommandRun(uint8_t id){return UiPort_RunCommand(id,UI_ACTION_SCREEN);}
static UiResult DefaultRun(uint8_t id){
 if(Busy())return UI_BUSY;
 if(id==0){
  int32_t open,close;
  if(!StsReady())return UI_OFFLINE;
  if(!FeetechStsControl_GetParamValue(FEETECH_STS_PARAM_OPEN_ANGLE,&open)||!FeetechStsControl_GetParamValue(FEETECH_STS_PARAM_CLOSE_ANGLE,&close))return UI_FAILED;
  FeetechStsControl_ResetAllParams();
  (void)FeetechStsControl_SetParamValue(FEETECH_STS_PARAM_OPEN_ANGLE,open);
  (void)FeetechStsControl_SetParamValue(FEETECH_STS_PARAM_CLOSE_ANGLE,close);
  return UI_ACCEPTED;
 }
 if(id==1)return MotorService_ResetParameters()?UI_ACCEPTED:UI_BUSY;
 if(id==2)return UiSettings_Save();
 if(id==3)return UiSettings_Load();
 if(id==4)return UiSettings_Clear();
 return UI_FAILED;
}
static void StatusLine(uint8_t tab,uint8_t line,char *out,uint32_t size){
 const FeetechStsFeedback_t *f=FeetechStsBus_GetFeedback();const BoardCameraStatus *c=BoardCamera_GetStatus();
 const BoardStorageStatus *storage=BoardStorage_GetStatus();MotorStatus m;MotorService_Snapshot(&m);
 if(tab==0){switch(line){
 case 0:snprintf(out,size,"CPU %lu MHz",(unsigned long)(HAL_RCC_GetSysClockFreq()/1000000U));break;
 case 1:snprintf(out,size,"LCD:%u RTC:%u USB:%u",Integration_GetStatus()->lcd_bus_ready,Integration_GetStatus()->rtc_ready,UsbConsole_Connected());break;
 case 2:snprintf(out,size,"SD:%u FS:%u QSPI:%u",storage->sd_ready,storage->fs_ready,storage->qspi_ready);break;
 case 3:snprintf(out,size,"STS:%u Stack:%u",f->online,App_MotionReady());break;
 case 4:snprintf(out,size,"CAM F:%lu E:%lu",(unsigned long)c->frames,(unsigned long)c->errors);break;
 case 5:snprintf(out,size,"CAM timeout:%lu",(unsigned long)c->timeouts);break;
 case 6:snprintf(out,size,"Motor:%u Driver:%u",m.control.enabled,m.driver);break;
 case 7:snprintf(out,size,"Fault:%u LoopMax:%lu",(unsigned)m.control.fault,(unsigned long)Integration_GetStatus()->loop_max_ms);break;
 default:snprintf(out,size,"Boot:%u seq:%lu",UiSettings_HasProfile(),(unsigned long)UiSettings_Sequence());break;}
 }else if(tab==1){switch(line){
 case 0:snprintf(out,size,"Online:%u PosValid:%u",f->online,f->position_valid);break;
 case 1:snprintf(out,size,"Pos:%u Angle10:%ld",f->position,(long)(f->angle_deg*10.0f));break;
 case 2:snprintf(out,size,"Voltage:%u mV",f->voltage_mv);break;
 case 3:snprintf(out,size,"Current:%u mA",f->current_ma);break;
 case 4:snprintf(out,size,"Load:%d Moving:%u",f->load,f->moving);break;
 case 5:snprintf(out,size,"Device error:%u",f->error);break;
 case 6:snprintf(out,size,"RawRx:%lu",(unsigned long)FeetechStsBus_RawRxBytes());break;
 case 7:snprintf(out,size,"State:%s",App_MotionReady()?FeetechStsControl_GetStateText():"OFFLINE");break;
 default:snprintf(out,size,"Owner:%s",App_MotionReady()?FeetechStsControl_GetOwnerText():"NONE");break;}
 }else if(tab==2){switch(line){
 case 0:snprintf(out,size,"Ready:%u Driver:%u",m.ready,m.driver);break;
 case 1:snprintf(out,size,"Enabled:%u Fault:%u",m.control.enabled,(unsigned)m.control.fault);break;
 case 2:snprintf(out,size,"Target:%ld RPM",(long)m.control.target_rpm);break;
 case 3:snprintf(out,size,"RPM x10:%ld",(long)(m.control.measured_rpm*10));break;
 case 4:snprintf(out,size,"Direction:%d PWM:%d",m.control.requested_direction,m.control.pwm);break;
 case 5:snprintf(out,size,"Count:%lld",(long long)m.control.total_counts);break;
 case 6:snprintf(out,size,"CPR x1000:%lu",(unsigned long)(m.control.config.counts_per_rev*1000.0f));break;
 case 7:snprintf(out,size,"Blocked:%lu",(unsigned long)m.blocked_starts);break;
 default:snprintf(out,size,"Knob invalid:%lu",(unsigned long)m.knob_invalid);break;}
 }else if(line==0)snprintf(out,size,"R:%u M:%u V:%u %s %s",c->rotation*90U,c->mirror,c->flip,c->frozen?"STILL":"LIVE",c->edges?"EDGE":"RGB");
 else if(line==1)snprintf(out,size,"CAM:%u ON:%u F:%lu",c->ready,c->enabled,(unsigned long)c->frames);
 else if(line==2)snprintf(out,size,"Timeout:%lu Error:%lu",(unsigned long)c->timeouts,(unsigned long)c->errors);
 else if(line==3)snprintf(out,size,"LCD rotation:%u",UiService_Rotation()*90U);
 else if(line==4)snprintf(out,size,"Still RAM valid:%u",c->frame_valid);
 else {const VisionAdvice *a=VisionLink_Advice(HAL_GetTick());
  if(line==5)snprintf(out,size,"PC advice:%u NO MOTION",a->valid);
  else if(line==6&&a->valid)snprintf(out,size,"U:%ld V:%ld S:%ld",(long)a->u,(long)a->v,(long)a->score);
  else if(line==7&&a->valid)snprintf(out,size,"A10:%ld Wpx:%ld",(long)a->angle10,(long)a->width);
  else snprintf(out,size,"%s",line==8?"Coordinates: raw pixels":"");
 }
}
static uint8_t CommandGroup(uint8_t id){if(id>=20)return 5;if((id>=8&&id<=10)||id==17)return 0;if(id>=3&&id<=6)return 1;if(id==1||id==2||(id>=11&&id<=16)||id==19)return 2;if(id==7)return 3;return 4;}
static UiResult HelpPrint(uint8_t id){return IntegrationConsole_RequestHelp(id)?UI_ACCEPTED:UI_BUSY;}
static const UiOperations ops={25,20,5,ParamName,ParamRead,ParamWrite,CommandName,CommandRun,DefaultName,DefaultRun,StatusLine,CommandGroup,9,IntegrationConsole_HelpName,HelpPrint};
const UiOperations *UiPort_Operations(void){return &ops;}

uint32_t UiPort_ViewFlags(void){const BoardCameraStatus *c=BoardCamera_GetStatus();return UiService_Rotation()|((uint32_t)c->rotation<<2)|((uint32_t)c->mirror<<4)|((uint32_t)c->flip<<5);}
void UiPort_ApplyViewFlags(uint32_t flags){UiService_SetRotation(flags&3U);BoardCamera_SetTransform((flags>>2)&3U,(flags>>4)&1U,(flags>>5)&1U);}
