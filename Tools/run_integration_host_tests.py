"""Run actual integration console + H7 record logic with a host HAL/flash model.
Windows virtual memory simulates the reserved flash addresses; no hardware is touched.
"""
from pathlib import Path
import json
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GCC = Path(r"C:\Dev-Cpp\MinGW64\bin\gcc.exe")
HAL = r'''
#ifndef HOST_HAL_H
#define HOST_HAL_H
#include <stdint.h>
#define __DMB() __asm__ volatile ("" ::: "memory")
typedef enum {HAL_OK,HAL_ERROR,HAL_BUSY,HAL_TIMEOUT} HAL_StatusTypeDef;
typedef struct {uint8_t Hours,Minutes,Seconds;} RTC_TimeTypeDef;
typedef struct {uint8_t Year,Month,Date;} RTC_DateTypeDef;
typedef struct {uint32_t TypeErase,Banks,Sector,NbSectors,VoltageRange;} FLASH_EraseInitTypeDef;
typedef struct {uint32_t OPTSR_CUR;} HostFlashRegs;
extern HostFlashRegs host_flash_regs;
#define FLASH (&host_flash_regs)
#define FLASH_OPTSR_SWAP_BANK_OPT (1UL<<31)
#define FLASH_TYPEERASE_SECTORS 1U
#define FLASH_BANK_2 2U
#define FLASH_SECTOR_4 4U
#define FLASH_SECTOR_5 5U
#define FLASH_SECTOR_6 6U
#define FLASH_SECTOR_7 7U
#define FLASH_VOLTAGE_RANGE_3 3U
#define FLASH_TYPEPROGRAM_FLASHWORD 1U
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *,uint32_t *);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t,uint32_t,uint32_t);
#endif
'''
HEADERS = {
    "motor_plot.h": "#include <stdint.h>\nstatic inline uint8_t MotorPlot_Enable(uint8_t on){(void)on;return 1;}static inline void MotorPlot_Report(void){}static inline uint8_t MotorPlot_IsEnabled(void){return 0;}\n",
    "board_config.h": '#include "platform_hal.h"\n#include "target_config.h"\n#include APP_BOARD_PROFILE_HEADER\n',
    "app_main.h": "static inline unsigned char App_MotionReady(void){return 0;}\n",

    "vision_link.h": "#include <stdint.h>\nstatic inline uint8_t VisionLink_Enable(uint8_t v){return v<=1;}static inline uint8_t VisionLink_SetFps(uint32_t v){return v>=1&&v<=5;}static inline void VisionLink_Report(void){}static inline void VisionLink_Clear(void){}static inline uint8_t VisionLink_Accept(uint32_t s,int32_t u,int32_t v,int32_t a,int32_t w,int32_t c){(void)s;(void)u;(void)v;(void)a;(void)w;(void)c;return 0;}\n",
    "usb_console.h": "#include <stdint.h>\nstatic inline void UsbConsole_Write(const uint8_t*p,uint16_t n){(void)p;(void)n;}\n",
    "bsp_key.h": '#include "input_control.h"\ntypedef enum {Key_Task1=1,Key_Task2,Key_Task3,Key_Task4,Key_Task5,Key_Task6} KeyCode_t;uint8_t Key_GetFlag(KeyCode_t);\n',
    "motor_service.h": "#include <stdint.h>\nuint8_t MotorService_IsRunning(void);uint8_t MotorService_Enable(uint8_t);uint8_t MotorService_SetTarget(uint16_t);void MotorService_Reverse(void);void MotorService_Stop(void);void MotorService_ClearFault(void);uint8_t MotorService_SetCpr(uint32_t);uint8_t MotorService_SetPid(uint32_t,uint32_t,uint32_t);uint8_t MotorService_ZeroCount(void);void MotorService_Report(void);uint8_t MotorService_SimulateKnob(int16_t);\n",
    "servo_p3.h": "#include <stdint.h>\nuint8_t ServoP3_ProbeAt(uint8_t,uint32_t);void ServoP3_SetTelemetry(uint8_t);\n#include <stddef.h>\ntypedef struct {uint8_t reported_id,state,online,scan_active,last_error;uint32_t mcu_baud,responses,timeouts,parse_errors,tx_errors;} ServoP3Status;\nstatic inline const ServoP3Status *ServoP3_GetStatus(void){static const ServoP3Status x={0};return &x;}static inline uint8_t ServoP3_StartScan(uint32_t x){return x>=9600&&x<=1000000;}\n",
    "feetech_sts_control.h": "#include <stdint.h>\nuint8_t FeetechStsControl_IsMotionState(void);\n#define FEETECH_STS_OWNER_VOFA 3\n#define FEETECH_STS_OWNER_USB_DIRECT 5\nstatic inline HAL_StatusTypeDef FeetechStsControl_CommandAngle(float a,int o){(void)a;(void)o;return HAL_ERROR;}\n#define FEETECH_STS_PARAM_PRINT_ENABLE 19\nuint8_t FeetechStsControl_SetParamValue(int,int32_t);\n",
    "pwm_servo_control.h": "typedef enum {PWM_SERVO_ID_1,PWM_SERVO_ID_2} PwmServoId_t;\nHAL_StatusTypeDef PwmServoControl_SetAngle(PwmServoId_t,float);\n",
    "board_camera.h": '''#include "platform_hal.h"
typedef struct {uint8_t ready,enabled,rotation,mirror,flip,edges;} BoardCameraStatus;
const BoardCameraStatus *BoardCamera_GetStatus(void);
HAL_StatusTypeDef BoardCamera_SetPclk(uint8_t);
void BoardCamera_Enable(uint8_t); HAL_StatusTypeDef BoardCamera_Init(void);
HAL_StatusTypeDef BoardCamera_Snapshot(void);void BoardCamera_Resume(void);void BoardCamera_SetTransform(uint8_t,uint8_t,uint8_t);void BoardCamera_SetEdges(uint8_t);
''',
    "board_storage.h": '''#include "platform_hal.h"
HAL_StatusTypeDef BoardStorage_Remount(void);
HAL_StatusTypeDef BoardStorage_FileTest(void); HAL_StatusTypeDef BoardStorage_SdReadTest(void);
HAL_StatusTypeDef BoardStorage_QspiTest(void); HAL_StatusTypeDef BoardStorage_QspiMap(uint8_t);
''',
    "board_peripherals.h": '''#include "platform_hal.h"
HAL_StatusTypeDef BoardPwm_SetChannelUs(uint8_t,uint16_t);
HAL_StatusTypeDef BoardPwm_SetUs(uint16_t);void BoardPwm_Stop(void);
HAL_StatusTypeDef BoardRtc_Get(RTC_TimeTypeDef *,RTC_DateTypeDef *);
HAL_StatusTypeDef BoardRtc_Set(uint16_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
''',
    "dcmi_ov5640.h": '''#include <stdint.h>
int8_t OV5640_AF_Download_Firmware(void);void OV5640_AF_Trigger_Constant(void);
''',
    "feetech_sts_bus.h": '''#include <stdint.h>
typedef struct {uint8_t moving,online,error;} FeetechStsFeedback_t;
const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void);
static inline uint32_t FeetechStsBus_RawRxBytes(void){return 0;}
''',
}
TEST = r'''
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "platform_hal.h"
#include "integration_console.h"
#include "claw_limit_storage.h"
#include "board_camera.h"
#include "feetech_sts_bus.h"
#include "ui_port.h"
#include "input_control.h"

HAL_StatusTypeDef BoardCamera_SetPclk(uint8_t d){(void)d;return HAL_OK;}
uint8_t ServoP3_ProbeAt(uint8_t id,uint32_t baud){(void)id;(void)baud;return 1;}
static BoardCameraStatus camera={.ready=1,.enabled=1};
static uint8_t telemetry,stream,claw_print,rotation;static unsigned log_calls;
static char log_text[32768];static unsigned log_length;static InputMode mode;static uint8_t masks[2];static int knob_steps;static unsigned param_writes,claw_action;
static uint8_t grants;
uint8_t Input_DebugEnabled(uint8_t t){return (grants>>t)&1;}uint8_t Input_DebugSet(uint8_t t,uint8_t on){if(on)grants|=1U<<t;else grants&=~(1U<<t);return 1;}
InputMode Input_GetMode(void){return mode;}uint8_t Input_SetMode(InputMode m){mode=m;return 1;}
uint8_t Input_SourceMask(uint8_t src,uint32_t now){(void)now;return masks[src];}
uint8_t Input_Pulse(uint8_t src,uint8_t mask,uint32_t ms,uint32_t now){(void)ms;(void)now;masks[src]|=mask;return mode!=INPUT_PHYSICAL;}
uint8_t Input_Level(uint8_t src,uint8_t key,uint8_t down,uint32_t now){(void)now;if(down)masks[src]|=(1U<<(key-1));else masks[src]&=~(1U<<(key-1));return 1;}
void Input_Release(uint8_t src){masks[src]=0;}uint8_t Key_GetFlag(int key){(void)key;return 0;}
uint8_t MotorService_SimulateKnob(int16_t n){knob_steps+=n;return 1;}
UiResult UiSettings_Save(void){return UI_ACCEPTED;}UiResult UiSettings_Load(void){return UI_ACCEPTED;}UiResult UiSettings_Clear(void){return UI_ACCEPTED;}
static int32_t param_value[25];
static const char *ParamName(uint8_t id){(void)id;return "TEST";}
static UiResult ParamRead(uint8_t id,UiParameter *p){p->value=param_value[id];p->min=0;p->max=400;p->step=id<2?0:5;return UI_ACCEPTED;}
static UiResult ParamWrite(uint8_t id,int32_t value,int32_t expected){if(value<0||value>400||param_value[id]!=expected)return UI_BLOCKED;param_writes++;param_value[id]=value;return UI_ACCEPTED;}
static UiResult CommandRun(uint8_t id){claw_action=id;return UI_ACCEPTED;}
UiResult UiPort_RunCommand(uint8_t id,UiActionSource source){(void)source;return CommandRun(id);}
static const UiOperations mock_ops={.parameter_count=25,.parameter_name=ParamName,.parameter_read=ParamRead,.parameter_write=ParamWrite,.command_run=CommandRun};
const UiOperations *UiPort_Operations(void){return &mock_ops;}

void ServoP3_SetTelemetry(uint8_t on){telemetry=on;}
uint8_t FeetechStsControl_SetParamValue(int id,int32_t v){assert(id==19);claw_print=(uint8_t)v;return 1;}
uint8_t UiService_Rotation(void){return rotation;}void UiService_SetRotation(uint8_t r){rotation=r&3;}
HAL_StatusTypeDef BoardCamera_Snapshot(void){return HAL_OK;}void BoardCamera_Resume(void){}
void BoardCamera_SetTransform(uint8_t r,uint8_t m,uint8_t f){camera.rotation=r&3;camera.mirror=m;camera.flip=f;}
void BoardCamera_SetEdges(uint8_t e){camera.edges=e;}
static uint8_t motor_on;static uint16_t motor_target;
uint8_t MotorService_IsRunning(void){return motor_on;}
uint8_t MotorService_Enable(uint8_t on){motor_on=on;return 1;}
uint8_t MotorService_SetTarget(uint16_t rpm){if(rpm>400)return 0;motor_target=rpm;return 1;}
void MotorService_Reverse(void){}void MotorService_Stop(void){motor_on=0;}
void MotorService_ClearFault(void){}uint8_t MotorService_SetCpr(uint32_t v){return v>0;}
uint8_t MotorService_SetPid(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;return 1;}
uint8_t MotorService_ZeroCount(void){return !motor_on;}void MotorService_Report(void){}
static uint32_t tick,status_calls,lcd_calls,file_calls,pwm_calls,erase_calls;
static uint8_t erase_fail,program_partial;
HostFlashRegs host_flash_regs;

static FeetechStsFeedback_t servo;
uint32_t HAL_GetTick(void){return tick;}
int VendorLog(const char *format,...){va_list ap;va_start(ap,format);int n=vsnprintf(log_text+log_length,sizeof(log_text)-log_length,format,ap);va_end(ap);assert(n>0&&log_length+(unsigned)n<sizeof(log_text));log_length+=(unsigned)n;log_calls++;return n;}
void Integration_StatusReport(void){status_calls++;}
void Integration_SetStream(uint8_t e){stream=e;}
void Integration_LcdTest(void){lcd_calls++;}
const BoardCameraStatus *BoardCamera_GetStatus(void){return &camera;}
void BoardCamera_Enable(uint8_t e){camera.enabled=e;}
HAL_StatusTypeDef BoardCamera_Init(void){return HAL_OK;}
const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void){return &servo;}
uint8_t FeetechStsControl_IsMotionState(void){return servo.moving;}
HAL_StatusTypeDef BoardStorage_Remount(void){return HAL_OK;}
HAL_StatusTypeDef BoardStorage_FileTest(void){file_calls++;return HAL_OK;}
HAL_StatusTypeDef BoardStorage_SdReadTest(void){return HAL_OK;}
HAL_StatusTypeDef BoardStorage_QspiTest(void){return HAL_OK;}
HAL_StatusTypeDef BoardStorage_QspiMap(uint8_t e){(void)e;return HAL_OK;}
HAL_StatusTypeDef PwmServoControl_SetAngle(int id,float a){(void)id;(void)a;pwm_calls++;return HAL_OK;}
HAL_StatusTypeDef BoardPwm_SetChannelUs(uint8_t ch,uint16_t p){(void)ch;(void)p;pwm_calls++;return HAL_OK;}
HAL_StatusTypeDef BoardPwm_SetUs(uint16_t p){(void)p;pwm_calls++;return HAL_OK;}
void BoardPwm_Stop(void){}
HAL_StatusTypeDef BoardRtc_Get(RTC_TimeTypeDef *t,RTC_DateTypeDef *d){memset(t,0,sizeof(*t));memset(d,0,sizeof(*d));return HAL_OK;}
HAL_StatusTypeDef BoardRtc_Set(uint16_t y,uint8_t m,uint8_t d,uint8_t h,uint8_t min,uint8_t s){(void)y;(void)m;(void)d;(void)h;(void)min;(void)s;return HAL_OK;}
int8_t OV5640_AF_Download_Firmware(void){return 0;}
void OV5640_AF_Trigger_Constant(void){}
HAL_StatusTypeDef HAL_FLASH_Unlock(void){return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void){return HAL_OK;}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *err){
 uint32_t address=0x08100000U+e->Sector*0x20000U;
 assert(e->Banks==FLASH_BANK_2 && e->NbSectors==1 && (e->Sector==6 || e->Sector==7));
 erase_calls++;if(erase_fail){*err=e->Sector;return HAL_ERROR;}
 memset((void *)(uintptr_t)address,0xff,0x20000U);return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t addr,uint32_t source){
 assert(type==FLASH_TYPEPROGRAM_FLASHWORD && !(addr&31U));
 memcpy((void *)(uintptr_t)addr,(const void *)(uintptr_t)source,program_partial?16:32);
 return program_partial?HAL_ERROR:HAL_OK;
}
static void send_line(const char *s,uint8_t ch){while(*s)assert(IntegrationConsole_PushByte((uint8_t)*s++,ch,1));}
unsigned ui_restore_calls;
void UiService_LcdTest(uint8_t active){if(!active)ui_restore_calls++;}
int main(void){
 uint32_t drops,erases;unsigned i;ClawLimitStorageData_t data;
 void *flash=VirtualAlloc((void *)0x081c0000,0x40000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 assert(flash==(void *)0x081c0000);memset(flash,0xff,0x40000);
 assert(ClawLimitStorage_Load(&data)==HAL_ERROR);
 assert(ClawLimitStorage_Save(100,1000)==HAL_OK);assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==1 && data.open_position==100);
 assert(ClawLimitStorage_Save(200,1200)==HAL_OK);assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==2);
 erase_fail=1;assert(ClawLimitStorage_Save(300,1400)==HAL_ERROR);erase_fail=0;
 assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==2 && data.open_position==200);
 program_partial=1;assert(ClawLimitStorage_Save(300,1400)==HAL_ERROR);program_partial=0;
 assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==2);
 assert(ClawLimitStorage_Save(300,1400)==HAL_OK);assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==3);
 ((uint32_t *)flash)[7]^=1U;assert(ClawLimitStorage_Load(&data)==HAL_OK && data.sequence==2);
 erases=erase_calls;assert(ClawLimitStorage_Save(5000,100)==HAL_ERROR);assert(ClawLimitStorage_Save(100,100)==HAL_ERROR);assert(erase_calls==erases);
 assert(ClawLimitStorage_Clear()==HAL_OK);assert(ClawLimitStorage_Load(&data)==HAL_OK && data.limits_valid==0 && data.sequence==3);
 assert(ClawLimitStorage_Load(0)==HAL_ERROR);
 erases=erase_calls;host_flash_regs.OPTSR_CUR=FLASH_OPTSR_SWAP_BANK_OPT;
 assert(ClawLimitStorage_Save(100,1000)==HAL_ERROR && erase_calls==erases);host_flash_regs.OPTSR_CUR=0;
 puts("PASS FLASH: alternating slots, erase failure, partial program/power loss, CRC corruption fallback, clear record, invalid range");
 assert(!IntegrationConsole_PushByte('@',0,0)); /* binary payload cannot start a console line */
 assert(!IntegrationConsole_PushByte(0xff,0,1));
 send_line("@status\n",0);send_line("@lcd test\n",1);IntegrationConsole_Task();assert(status_calls==1 && lcd_calls==1);
 send_line("@status\n",0);send_line("@pwm 1500\n",0);IntegrationConsole_Task();assert(status_calls==2 && !pwm_calls); /* full mailbox preserved */
 drops=IntegrationConsole_Dropped();send_line("@",0);for(i=0;i<100;i++)assert(IntegrationConsole_PushByte('a',0,1));send_line("\n",0);
 assert(IntegrationConsole_Dropped()==drops+1);IntegrationConsole_Task();assert(status_calls==2);
 send_line("@abandoned",0);tick=201;assert(!IntegrationConsole_PushByte(0xff,0,1)); /* restore binary parsing */
 send_line("@status\n",0);IntegrationConsole_Task();assert(status_calls==3);
 send_line("@camera on\n",0);IntegrationConsole_Task();assert(ui_restore_calls==1);
 send_line("@motor rpm 100\n",0);IntegrationConsole_Task();assert(motor_target==100);
 send_line("@motor enable\n",0);IntegrationConsole_Task();assert(motor_on);
 send_line("@motor stop\n",0);IntegrationConsole_Task();assert(!motor_on);
 servo.moving=1;send_line("@sd filetest\n",0);IntegrationConsole_Task();assert(!file_calls);
 servo.moving=0;motor_on=1;send_line("@sd filetest\n",0);IntegrationConsole_Task();assert(!file_calls);motor_on=0;send_line("@sd filetest\n",0);IntegrationConsole_Task();assert(file_calls==1);
 send_line("@pwm 65536\n",0);IntegrationConsole_Task();assert(!pwm_calls);
 send_line("@pwm 1500\n",0);IntegrationConsole_Task();assert(pwm_calls==1);
 telemetry=stream=claw_print=1;send_line("@quiet\n",0);IntegrationConsole_Task();assert(!telemetry&&!stream&&!claw_print);
 log_calls=0;IntegrationConsole_Task();assert(!log_calls);assert(IntegrationConsole_RequestHelp(2));assert(!IntegrationConsole_RequestHelp(1));
 for(i=0;i<100;i++){tick+=25;unsigned previous_calls=log_calls;IntegrationConsole_Task();assert(log_calls-previous_calls<=1);}assert(log_calls==MOTOR_HELP_LINES);log_calls=0;IntegrationConsole_Task();assert(!log_calls);
 send_line("@camera rotate\n",0);IntegrationConsole_Task();assert(camera.rotation==1);send_line("@camera mirror\n",0);IntegrationConsole_Task();assert(camera.mirror==1);
 send_line("@lcd rotate\n",0);IntegrationConsole_Task();assert(rotation==1);
 send_line("@key 3 tap\n",0);IntegrationConsole_Task();assert(masks[0]==4);send_line("@keys 12 100\n",1);IntegrationConsole_Task();assert(masks[1]==12);
 send_line("@keys release\n",0);IntegrationConsole_Task();assert(!masks[0]&&masks[1]==12);
 send_line("@key 99 tap\n",0);IntegrationConsole_Task();assert(!masks[0]);send_line("@key 3 tap extra\n",0);IntegrationConsole_Task();assert(!masks[0]);
 log_length=0;log_text[0]=0;send_line("@servo status\n",0);IntegrationConsole_Task();assert(strstr(log_text,"configured=1")&&strstr(log_text,"broadcast=254"));
 log_length=0;log_text[0]=0;send_line("@servo scan 1000000\n",0);IntegrationConsole_Task();assert(strstr(log_text,"accepted=1"));
 log_length=0;log_text[0]=0;send_line("@servo scan 1000001\n",0);IntegrationConsole_Task();assert(strstr(log_text,"accepted=0"));
 log_length=0;log_text[0]=0;send_line("@servo scan 1000000 extra\n",0);IntegrationConsole_Task();assert(strstr(log_text,"accepted=0"));
 log_length=0;log_text[0]=0;send_line("@servo angle 90\n",0);IntegrationConsole_Task();assert(strstr(log_text,"accepted=0")); /* offline guard */
 send_line("@key 7 tap\n",0);IntegrationConsole_Task();assert(masks[0]==64);send_line("@keys release\n",0);IntegrationConsole_Task();
 send_line("@keys 128 100\n",0);IntegrationConsole_Task();assert(!masks[0]);
 send_line("@debug sts on\n",0);IntegrationConsole_Task();send_line("@debug motor on\n",0);IntegrationConsole_Task();assert(grants==3);send_line("@debug sts off\n",0);IntegrationConsole_Task();assert(grants==2);
 send_line("@knob -1\n",0);IntegrationConsole_Task();assert(knob_steps==-1);send_line("@knob 214748364899999\n",0);IntegrationConsole_Task();assert(knob_steps==-1);
 send_line("@param set 21 100\n",0);IntegrationConsole_Task();assert(param_value[21]==100&&param_writes==1);
 send_line("@param set 21 401\n",0);IntegrationConsole_Task();assert(param_writes==1);send_line("@param set 0 50\n",0);IntegrationConsole_Task();assert(param_writes==1);
 send_line("@claw close\n",0);IntegrationConsole_Task();assert(claw_action==10);
 log_length=0;log_text[0]=0;send_line("@help\n",0);IntegrationConsole_Task();for(i=0;i<10;i++){tick+=25;IntegrationConsole_Task();}assert(strstr(log_text,"@help keys")&&strstr(log_text,"@help params"));
 log_length=0;log_text[0]=0;send_line("@param list\n",0);IntegrationConsole_Task();for(i=0;i<51;i++){tick+=25;IntegrationConsole_Task();}assert(strstr(log_text,"@param set 21 100")&&strstr(log_text,"@param get 0"));
 puts("PASS mapped console: strict keys/masks, source release, detents, numeric overflow/trailing-token rejection, shared param bounds/read-only, executable help index/list");
 puts("PASS quiet/help: explicit paced category output, idle silence, all streams disabled, independent camera/LCD controls");
 puts("PASS CONSOLE: binary isolation, dual-channel lines, full mailbox, overflow, timeout recovery, moving-state maintenance gate, PWM range");
 VirtualFree(flash,0,MEM_RELEASE);return 0;
}
'''

PWM_TEST = r'''
#include <assert.h>
#include <stdio.h>
#include "pwm_servo_control.h"
#include "tim.h"
static uint16_t pulse[2];static unsigned writes,starts[2];
HAL_StatusTypeDef TIM3_SERVO_PWM_SetPulseUs(Tim3ServoPwmChannel_t c,uint16_t us) {
 assert((unsigned)c<2 && us>=500 && us<=2500);pulse[c]=us;writes++;return HAL_OK;
}
HAL_StatusTypeDef TIM3_SERVO_PWM_StartChannel(Tim3ServoPwmChannel_t c){starts[c]++;return HAL_OK;}
int main(void){
 PwmServoControl_Init();assert(!writes);
 assert(PwmServoControl_SetAngle(PWM_SERVO_ID_1,90)==HAL_OK && pulse[0]==1500 && starts[0]==1);
 assert(PwmServoControl_SetAngle(PWM_SERVO_ID_2,180)==HAL_OK && pulse[1]==2500 && starts[1]==1);
 assert(PwmServoControl_SetAngle(PWM_SERVO_ID_1,-100)==HAL_OK && pulse[0]==500);
 assert(PwmServoControl_SetAngle(PWM_SERVO_ID_2,400)==HAL_OK && pulse[1]==2500);
 assert(PwmServoControl_SetAngle((PwmServoId_t)255,30)==HAL_ERROR);
 PwmServoControl_Init();writes=0;starts[0]=starts[1]=0;
 PwmServoControl_Task();assert(!writes);
 PwmServoControl_SetVirtualKeyLevel(PWM_SERVO_ID_1,1);PwmServoControl_Task();assert(writes==1 && pulse[0]==500);
 PwmServoControl_Task();assert(writes==1);
 PwmServoControl_SetVirtualKeyLevel(PWM_SERVO_ID_1,0);PwmServoControl_Task();
 PwmServoControl_SetVirtualKeyLevel(PWM_SERVO_ID_1,1);PwmServoControl_Task();assert(writes==2 && pulse[0]==1500);
 PwmServoControl_SetVirtualKeyLevel(PWM_SERVO_ID_2,1);PwmServoControl_Task();assert(writes==3 && pulse[1]==500);
 puts("PASS PWM: no startup output, independent channels, angle clamping, invalid ID, virtual key edges/open-close toggles");return 0;
}
'''

def main():
    app = ROOT / "project/App"
    with tempfile.TemporaryDirectory(prefix="h743-integration-test-") as folder:
        tmp = Path(folder)
        (tmp / "platform_hal.h").write_text(HAL, encoding="utf-8")
        for name, text in HEADERS.items():
            (tmp / name).write_text(text, encoding="utf-8")
        examples=json.loads((ROOT/'Docs/输入映射/指令帮助目录.json').read_text(encoding='utf-8'))
        text_examples=[x['example'] for g in examples for x in g['items'] if x['format']=='text']
        calls=''
        for example in text_examples:
            calls+='send_line("@quiet\\n",0);IntegrationConsole_Task();log_length=0;log_text[0]=0;send_line('+json.dumps(example+'\n')+',0);IntegrationConsole_Task();assert(!strstr(log_text,"unknown command"));\n'
        test=TEST.replace(' VirtualFree(flash,0,MEM_RELEASE);return 0;',calls+' puts("PASS catalog: all printable text examples recognized by actual parser with mocked hardware");\n VirtualFree(flash,0,MEM_RELEASE);return 0;')
        test=test.replace('MOTOR_HELP_LINES',str(1+2*len(examples[2]['items'])))
        (tmp / "test.c").write_text(test, encoding="utf-8")
        command = [str(GCC), "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unknown-pragmas",
                   "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-Wl,--image-base,0x400000"]
        for path in [tmp, app / "config", app / "platform", app / "bsp", app / "modules/integration", app / "modules/claw_core", app / "modules/ui"]:
            command += ["-I", str(path)]
        command += [str(tmp / "test.c"), str(app / "modules/integration/integration_console.c"),
                    str(app / "platform/claw_limit_storage_h7.c"), str(app / "platform/board_nv_storage.c"), "-o", str(tmp / "test.exe")]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode:
            raise SystemExit(build.stdout + build.stderr)
        run = subprocess.run([str(tmp / "test.exe")], capture_output=True, text=True)
        pwm = tmp / "pwm"
        pwm.mkdir()
        (pwm / "platform_hal.h").write_text(HAL.replace("#endif", "typedef struct { int unused; } TIM_HandleTypeDef;\n#endif"), encoding="utf-8")
        (pwm / "main.h").write_text('#include "platform_hal.h"\n', encoding="utf-8")
        (pwm / "test.c").write_text(PWM_TEST, encoding="utf-8")
        pwmcmd = [str(GCC), "-std=c99", "-Wall", "-Wextra", "-Werror"]
        for inc in [pwm, app / "drivers/Servo", app / "platform", app / "bsp", app / "config", app / "platform/boards/lxb743zi_p1"]:
            pwmcmd += ["-I", str(inc)]
        pwmcmd += [str(pwm / "test.c"), str(app / "drivers/Servo/pwm_servo_control.c"), "-o", str(pwm / "test.exe")]
        pc = subprocess.run(pwmcmd, capture_output=True, text=True)
        if pc.returncode:
            raise SystemExit(pc.stdout + pc.stderr)
        pr = subprocess.run([str(pwm / "test.exe")], capture_output=True, text=True)
        run.stdout += pr.stdout
        run.stderr += pr.stderr
        run.returncode = run.returncode or pr.returncode
        # Exercise the actual shared logger, not only console mocks.
        logdir=tmp/"logger";logdir.mkdir()
        (logdir/"vofa_protocol.h").write_text('#include <stdint.h>\nint VOFA_Protocol_SendRaw(const uint8_t*,uint16_t);\n')
        (logdir/"usb_console.h").write_text('#include <stdint.h>\nvoid UsbConsole_Write(const uint8_t*,uint16_t);\n')
        (logdir/"test.c").write_text('#include <assert.h>\n#include "vendor_log.h"\nstatic int host,usb;int VOFA_Protocol_SendRaw(const uint8_t*p,uint16_t n){(void)p;host+=n;return 0;}void UsbConsole_Write(const uint8_t*p,uint16_t n){(void)p;usb+=n;}int main(void){VendorLog_Enable(0);VendorLog("boot");assert(!host&&!usb);VendorLog_Enable(1);VendorLog("reply");assert(host==5&&usb==5);puts("PASS startup log gate: all shared init output suppressed, explicit replies restored");return 0;}')
        lc=[str(GCC),'-std=c99','-Wall','-Wextra','-Werror','-I',str(logdir),'-I',str(app/'modules/integration'),str(logdir/'test.c'),str(app/'modules/integration/vendor_log.c'),'-o',str(logdir/'test.exe')]
        lr=subprocess.run(lc,capture_output=True,text=True)
        if lr.returncode:raise SystemExit(lr.stdout+lr.stderr)
        lr=subprocess.run([str(logdir/'test.exe')],capture_output=True,text=True)
        run.stdout+=lr.stdout;run.stderr+=lr.stderr;run.returncode=run.returncode or lr.returncode
        result = {"scope": "host-only HAL and flash fault model; no target-board test",
                  "exit_code": run.returncode, "stdout": run.stdout, "stderr": run.stderr}
        (ROOT / "Docs/整合交付/整合主机测试.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(run.stdout, end="")
        if run.returncode:
            raise SystemExit(run.returncode)

if __name__ == "__main__":
    main()

