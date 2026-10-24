#include "integration_console.h"
#include "integration_service.h"
#include "integration_config.h"
#include "board_storage.h"
#include "board_camera.h"
#include "board_peripherals.h"
#include "vendor_log.h"
#include "feetech_sts_bus.h"
#include "feetech_sts_control.h"
#include "pwm_servo_control.h"
#include "servo_p3.h"
#include "app_main.h"
#include "board_config.h"
#include "motor_service.h"
#include "motor_plot.h"
#include "ui_service.h"
#include "ui_port.h"
#include "ui_settings.h"
#include "bsp_key.h"
#include "console_help.h"
#include "vision_link.h"
#include "usb_console.h"
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#if BOARD_CAMERA_SENSOR == 5640
#include "dcmi_ov5640.h"
#endif
#include <string.h>
#include <stdio.h>

#define LINE_SIZE 96U
typedef struct { char line[LINE_SIZE]; uint8_t index, active, discard; volatile uint8_t ready; uint32_t last_tick; } ConsoleLine;
static ConsoleLine s_input[2];
static volatile uint32_t s_dropped[2];

uint8_t IntegrationConsole_PushByte(uint8_t b, uint8_t channel, uint8_t allow_start)
{
    ConsoleLine *c;
    if (channel > 1) return 0;
    c = &s_input[channel];
    if (c->active && (uint32_t)(HAL_GetTick() - c->last_tick) > 200U) {
        c->active = 0; c->discard = 0; c->index = 0; s_dropped[channel]++;
    }
    c->last_tick = HAL_GetTick();
    if (!c->active) {
        if (!allow_start || b != '@') return 0;
        c->active = 1; c->index = 0; c->discard = c->ready;
        if (c->discard) s_dropped[channel]++;
        return 1;
    }
    if (b == '\r' || b == '\n') {
        if (!c->discard) { c->line[c->index] = 0; __DMB(); c->ready = 1; }
        c->active = 0; return 1;
    }
    if (c->discard) return 1;
    if (b < 32 || b > 126 || c->index >= LINE_SIZE - 1U) { c->discard = 1; s_dropped[channel]++; return 1; }
    c->line[c->index++] = (char)b;
    return 1;
}

uint32_t IntegrationConsole_Dropped(void) { uint32_t a=s_dropped[0]; uint32_t b=s_dropped[1]; return a+b; }

/* Help is explicit and paced one short line per loop; idle console stays silent. */
static const char *const help_alias[]={"system","claw","motor","camera","pwm","storage","binary","keys","params"};
static uint8_t help_group=255,help_line;
static uint32_t next_print;
static uint8_t param_index=255;
const char *IntegrationConsole_HelpName(uint8_t group){return group<CONSOLE_HELP_COUNT?help_names[group]:"?";}
uint8_t IntegrationConsole_RequestHelp(uint8_t group){if(group>=CONSOLE_HELP_COUNT||help_group!=255||param_index!=255)return 0;help_group=group;help_line=0;return 1;}
void IntegrationConsole_Quiet(void){
    (void)MotorPlot_Enable(0);
 Integration_SetStream(0);ServoP3_SetTelemetry(0);
 (void)FeetechStsControl_SetParamValue(FEETECH_STS_PARAM_PRINT_ENABLE,0);
 help_group=255;param_index=255;
}
static void HelpTask(void){
 if(help_group==255)return;
 if(help_group==CONSOLE_HELP_COUNT){VendorLog("@help %s\r\n",help_alias[help_line]);if(++help_line>=CONSOLE_HELP_COUNT)help_group=255;return;}
 VendorLog("%s\r\n",help_text[help_group][help_line]);
 if(!help_text[help_group][++help_line])help_group=255;
}
static uint8_t ParseInt(const char *s,int32_t *value){
 char *end;long n;errno=0;n=strtol(s,&end,10);
 if(!*s||*end||errno==ERANGE)return 0;
#if LONG_MAX > 2147483647L
 if(n < (-2147483647L-1L)||n>2147483647L)return 0;
#endif
 *value=(int32_t)n;return 1;
}
static void ParamTask(void){
 const UiOperations *ops=UiPort_Operations();UiParameter p;UiResult r;uint8_t id;
 if(param_index==255)return;id=param_index++;r=ops->parameter_read(id,&p);
 if(r==UI_ACCEPTED||r==UI_OFFLINE){
  VendorLog("# %u %s value=%ld min=%ld max=%ld step=%ld state=%u\r\n",id,ops->parameter_name(id),(long)p.value,(long)p.min,(long)p.max,(long)p.step,(unsigned)r);
  /* The next iteration emits the copyable command, keeping one line per loop. */
  param_index=(uint8_t)(id|128U);
 }else VendorLog("# param %u unavailable=%u\r\n",id,(unsigned)r);
 if(param_index==ops->parameter_count)param_index=255;
}
static void ParamCommandTask(void){
 const UiOperations *ops=UiPort_Operations();UiParameter p;UiResult r;uint8_t id;
 if(param_index==255||!(param_index&128U)){ParamTask();return;}
 id=param_index&127U;r=ops->parameter_read(id,&p);
 if(r==UI_ACCEPTED&&p.step>0)VendorLog("@param set %u %ld\r\n",id,(long)p.value);
 else VendorLog("@param get %u\r\n",id);
 param_index=(uint8_t)(id+1U);if(param_index>=ops->parameter_count)param_index=255;
}
static uint8_t HelpByToken(const char *s){
 int32_t number;uint8_t i;
 if(ParseInt(s,&number))return number>=0&&(uint32_t)number<CONSOLE_HELP_COUNT?(uint8_t)number:255;
 for(i=0;i<CONSOLE_HELP_COUNT;i++)if(!strcmp(s,help_alias[i]))return i;
 return 255;
}
/* Text simulation has its own source slots: releasing USB cannot release UART/GPIO. */
static void Execute(const char *s,uint8_t source)
{
    unsigned y, m, d, h, minute, second, us; HAL_StatusTypeDef result;
    RTC_TimeTypeDef t; RTC_DateTypeDef date;
    char a[24],b[24],c[24],extra[24];char tokens[7][24];int32_t values[6];uint8_t i;int count;int32_t x,z;uint8_t accepted;
    const UiOperations *ops=UiPort_Operations();UiParameter parameter;UiResult ui_result;
    uint8_t maintenance = !strncmp(s,"sd ",3) || !strncmp(s,"qspi ",5) || !strcmp(s,"camera retry") ||
                          !strncmp(s,"camera clock ",13) || !strcmp(s,"af") || !strcmp(s,"lcd test") || !strncmp(s,"rtc set ",8);
    if (maintenance && (FeetechStsBus_GetFeedback()->moving || FeetechStsControl_IsMotionState() || MotorService_IsRunning())) {
        VendorLog("BUSY: stop motion before maintenance command\r\n"); return;
    }
    if (!strcmp(s,"quiet")) IntegrationConsole_Quiet();
    else if (!strcmp(s,"telemetry on")) ServoP3_SetTelemetry(1);
    else if (!strcmp(s,"telemetry off")) ServoP3_SetTelemetry(0);
    else if (!strncmp(s,"help ",5)) {
        count=sscanf(s,"help %23s %23s",a,extra);
        if(count!=1||!IntegrationConsole_RequestHelp(HelpByToken(a)))VendorLog("help invalid/busy\r\n");
    }
    else if (!strcmp(s,"debug status")) VendorLog("DEBUG sts=%u motor=%u plot=%u; runtime grants only\r\n",Input_DebugEnabled(0),Input_DebugEnabled(1),MotorPlot_IsEnabled());
    else if (!strncmp(s,"debug ",6)) {
        count=sscanf(s,"debug %23s %23s %23s",a,b,extra);accepted=0;
        if(count==2 && (!strcmp(a,"sts")||!strcmp(a,"motor")) && (!strcmp(b,"on")||!strcmp(b,"off")))
            accepted=Input_DebugSet((uint8_t)!strcmp(a,"motor"),(uint8_t)!strcmp(b,"on"));
        VendorLog("debug accepted=%u; release shared keys first; grants do not start/stop motion\r\n",accepted);
    }
    else if (!strcmp(s,"input status")) VendorLog("INPUT mode=%u UARTmask=%u USBmask=%u K1..6=%u%u%u%u%u%u\r\n",(unsigned)Input_GetMode(),Input_SourceMask(0,HAL_GetTick()),Input_SourceMask(1,HAL_GetTick()),Key_GetFlag(Key_Task1),Key_GetFlag(Key_Task2),Key_GetFlag(Key_Task3),Key_GetFlag(Key_Task4),Key_GetFlag(Key_Task5),Key_GetFlag(Key_Task6));
    else if (!strncmp(s,"input mode ",11)) {
        count=sscanf(s,"input mode %23s %23s",a,extra);accepted=0;
        if(count==1&&!MotorService_IsRunning()&&!FeetechStsControl_IsMotionState()&&!FeetechStsBus_GetFeedback()->moving){
            if(!strcmp(a,"mixed"))accepted=Input_SetMode(INPUT_MIXED);
            else if(!strcmp(a,"physical"))accepted=Input_SetMode(INPUT_PHYSICAL);
            else if(!strcmp(a,"remote"))accepted=Input_SetMode(INPUT_REMOTE);
        }
        VendorLog("input mode accepted=%u; release keys/stop motion first\r\n",accepted);
    }
    else if (!strcmp(s,"keys release")){Input_Release(source);VendorLog("keys released source=%u\r\n",source);}
    else if (!strncmp(s,"key ",4)) {
        count=sscanf(s,"key %23s %23s %23s %23s",a,b,c,extra);accepted=0;
        if((count==2||count==3)&&ParseInt(a,&x)&&x>=1&&(uint32_t)x<=INPUT_KEY_COUNT){
            if(count==2&&!strcmp(b,"up"))accepted=Input_Level(source,(uint8_t)x,0,HAL_GetTick());
            else if(count==2&&!strcmp(b,"down"))accepted=Input_Level(source,(uint8_t)x,1,HAL_GetTick());
            else if(count==2&&!strcmp(b,"tap"))accepted=Input_Pulse(source,(uint8_t)(1U<<(x-1)),100,HAL_GetTick());
            else if(count==2&&!strcmp(b,"hold"))accepted=Input_Pulse(source,(uint8_t)(1U<<(x-1)),900,HAL_GetTick());
            else if(count==3&&!strcmp(b,"press")&&ParseInt(c,&z)&&z>=40&&z<=5000)accepted=Input_Pulse(source,(uint8_t)(1U<<(x-1)),(uint32_t)z,HAL_GetTick());
        }
        VendorLog("key accepted=%u; ids1..7, pulse40..5000ms\r\n",accepted);
    }
    else if (!strncmp(s,"keys ",5)) {
        count=sscanf(s,"keys %23s %23s %23s",a,b,extra);accepted=0;
        if(count==2&&ParseInt(a,&x)&&ParseInt(b,&z)&&x>=1&&(uint32_t)x<=INPUT_KEY_MASK&&z>=40&&z<=5000)accepted=Input_Pulse(source,(uint8_t)x,(uint32_t)z,HAL_GetTick());
        VendorLog("keys accepted=%u; mask1..127, pulse40..5000ms\r\n",accepted);
    }
    else if (!strncmp(s,"knob ",5)) {
        count=sscanf(s,"knob %23s %23s",a,extra);accepted=0;
        if(count==1&&ParseInt(a,&x)&&x>=-80&&x<=80&&x!=0)accepted=MotorService_SimulateKnob((int16_t)x);
        VendorLog("knob accepted=%u; detents=-80..80 excluding0\r\n",accepted);
    }
    else if (!strcmp(s,"param list")) {
        if(help_group!=255||param_index!=255)VendorLog("param list busy\r\n");else param_index=0;
    }
    else if (!strncmp(s,"param get ",10)) {
        count=sscanf(s,"param get %23s %23s",a,extra);
        if(count==1&&ParseInt(a,&x)&&x>=0&&x<ops->parameter_count){
            ui_result=ops->parameter_read((uint8_t)x,&parameter);
            if(ui_result==UI_ACCEPTED||ui_result==UI_OFFLINE)VendorLog("PARAM %ld %s value=%ld min=%ld max=%ld step=%ld state=%u\r\n",(long)x,ops->parameter_name((uint8_t)x),(long)parameter.value,(long)parameter.min,(long)parameter.max,(long)parameter.step,(unsigned)ui_result);
            else VendorLog("param unavailable=%u\r\n",(unsigned)ui_result);
        }else VendorLog("param invalid ID\r\n");
    }
    else if (!strncmp(s,"param set ",10)) {
        count=sscanf(s,"param set %23s %23s %23s",a,b,extra);ui_result=UI_BLOCKED;
        if(count==2&&ParseInt(a,&x)&&ParseInt(b,&z)&&x>=0&&x<ops->parameter_count){
            ui_result=ops->parameter_read((uint8_t)x,&parameter);
            if(ui_result==UI_ACCEPTED)ui_result=parameter.step>0?ops->parameter_write((uint8_t)x,z,parameter.value):UI_BLOCKED;
        }
        VendorLog("param result=%u (0=accepted,1=offline,2=busy,3=blocked,4=changed)\r\n",(unsigned)ui_result);
    }
    else if (!strcmp(s,"profile save")) VendorLog("profile save=%u\r\n",(unsigned)UiSettings_Save());
    else if (!strcmp(s,"profile load")) VendorLog("profile load=%u\r\n",(unsigned)UiSettings_Load());
    else if (!strcmp(s,"profile clear")) VendorLog("profile clear=%u\r\n",(unsigned)UiSettings_Clear());
    else if (!strcmp(s,"claw open")) VendorLog("claw open=%u\r\n",(unsigned)UiPort_RunCommand(9,source?UI_ACTION_USB:UI_ACTION_HOST));
    else if (!strcmp(s,"claw close")) VendorLog("claw close=%u\r\n",(unsigned)UiPort_RunCommand(10,source?UI_ACTION_USB:UI_ACTION_HOST));
    else if (!strcmp(s,"claw stop")) VendorLog("claw stop=%u\r\n",(unsigned)UiPort_RunCommand(8,source?UI_ACTION_USB:UI_ACTION_HOST));
    else if (!strcmp(s, "help")) {
        if(help_group!=255||param_index!=255)VendorLog("help busy\r\n");else {help_group=CONSOLE_HELP_COUNT;help_line=0;}
    } else if (!strcmp(s, "stream on")) Integration_SetStream(1);
    else if (!strcmp(s, "stream off")) Integration_SetStream(0);
    else if (!strcmp(s,"servo status")) {
        const ServoP3Status *p=ServoP3_GetStatus();
        VendorLog("SERVO configured=%u probed=%u state=%u online=%u stack=%u baud=%lu scan=%u error=%u broadcast=254\r\n",BOARD_SERVO_ID,p->reported_id,p->state,p->online,App_MotionReady(),(unsigned long)p->mcu_baud,p->scan_active,p->last_error);
        VendorLog("SERVO rxBytes=%lu replies=%lu timeout=%lu parser=%lu txErrors=%lu; ID0 is unicast\r\n",(unsigned long)FeetechStsBus_RawRxBytes(),(unsigned long)p->responses,(unsigned long)p->timeouts,(unsigned long)p->parse_errors,(unsigned long)p->tx_errors);
    }
    else if (!strncmp(s,"servo scan ",11)) {
        count=sscanf(s,"servo scan %23s %23s",a,extra);
        accepted=(count==1&&ParseInt(a,&x)&&x>=9600&&x<=1000000&&!App_MotionReady()&&!MotorService_IsRunning())?ServoP3_StartScan((uint32_t)x):0;
        VendorLog("servo scan accepted=%u; unicast READ ONLY ID0..253; nonconfigured ID cannot enable motion\r\n",accepted);
    }
    else if (!strncmp(s,"servo angle ",12)) {
        count=sscanf(s,"servo angle %23s %23s",a,extra);accepted=0;
        if(count==1&&ParseInt(a,&x)&&x>=0&&x<=270&&App_MotionReady()&&FeetechStsBus_GetFeedback()->online&&!FeetechStsBus_GetFeedback()->error)
            accepted=(FeetechStsControl_CommandAngle((float)x,source?FEETECH_STS_OWNER_USB_DIRECT:FEETECH_STS_OWNER_VOFA)==HAL_OK);
        VendorLog("servo angle accepted=%u; requires online/control/valid limits, units=degrees\r\n",accepted);
    }
    else if (sscanf(s, "servo probe %u %u", &y, &us) == 2) {
        if (y >= 254U || us < 9600U || us > 1000000U) VendorLog("probe invalid ID/rate\r\n");
        else VendorLog("read-only probe accepted=%u (nonconfigured ID never enables motion)\r\n", ServoP3_ProbeAt((uint8_t)y, (uint32_t)us));
    }
    else if (!strcmp(s,"video on")) VendorLog("video accepted=%u; USB image only\r\n",VisionLink_Enable(1));
    else if (!strcmp(s,"video off")) {VisionLink_Enable(0);VendorLog("video stopped\r\n");}
    else if (!strcmp(s,"video status")) VisionLink_Report();
    else if (!strncmp(s,"video fps ",10)) {
        count=sscanf(s,"video fps %23s %23s",a,extra);
        VendorLog("video fps accepted=%u\r\n",count==1&&ParseInt(a,&x)&&x>=1&&x<=5?VisionLink_SetFps((uint32_t)x):0);
    }
    else if (!strcmp(s,"vision status")) VisionLink_Report();
    else if (!strcmp(s,"vision clear")) {VisionLink_Clear();VendorLog("vision cleared\r\n");}
    else if (!strncmp(s,"vision target ",14)) {
        count=sscanf(s,"vision target %23s %23s %23s %23s %23s %23s %23s",tokens[0],tokens[1],tokens[2],tokens[3],tokens[4],tokens[5],tokens[6]);accepted=(count==6);
        for(i=0;i<6&&accepted;i++)if(!ParseInt(tokens[i],&values[i]))accepted=0;
        if(accepted && values[0]>=0)accepted=VisionLink_Accept((uint32_t)values[0],values[1],values[2],values[3],values[4],values[5]);else accepted=0;
        /* Frequent PC suggestions reply only to USB, preserving idle VOFA UART. */
        if(!source)VendorLog("vision target accepted=%u; advisory only\r\n",accepted);
        else if(!accepted){static const uint8_t msg[]="vision target rejected; stale/range/offline\r\n";UsbConsole_Write(msg,sizeof(msg)-1U);}
    }
    else if (!strcmp(s,"motor status")) MotorService_Report();
    else if (!strcmp(s,"motor plot on")) {if(MotorPlot_IsEnabled())accepted=1;else {IntegrationConsole_Quiet();accepted=MotorPlot_Enable(1);}VendorLog("motor plot accepted=%u; JustFloat HOST only; retry if TX busy\r\n",accepted);}
    else if (!strcmp(s,"motor plot off")) {(void)MotorPlot_Enable(0);VendorLog("motor plot stopped\r\n");}
    else if (!strcmp(s,"motor plot status")) MotorPlot_Report();
    else if (!strcmp(s,"motor disable")) VendorLog("motor disable=%u; setpoint retained\r\n",MotorService_Enable(0));
    else if (!strcmp(s,"motor enable")) VendorLog("motor enable=%u (check driver/ready/CPR/fault)\r\n",MotorService_Enable(1));
    else if (!strcmp(s,"motor stop")) MotorService_Stop();
    else if (!strcmp(s,"motor reverse")) MotorService_Reverse();
    else if (!strcmp(s,"motor clear")) MotorService_ClearFault();
    else if (!strcmp(s,"motor zero")) VendorLog("motor zero=%u\r\n",MotorService_ZeroCount());
    else if (sscanf(s,"motor rpm %u",&us)==1 && us<=65535U) VendorLog("motor target=%u\r\n",MotorService_SetTarget((uint16_t)us));
    else if (sscanf(s,"motor cpr %u",&us)==1) VendorLog("motor cpr=%u\r\n",MotorService_SetCpr(us));
    else if (sscanf(s,"motor pid %u %u %u",&y,&m,&d)==3) VendorLog("motor pid=%u\r\n",MotorService_SetPid(y,m,d));
    else if (!strcmp(s, "status")) {Integration_StatusReport();MotorService_Report();}
    else if (sscanf(s, "camera clock %u", &us) == 1 && us <= 16) VendorLog("camera clock=%d (0=vendor auto)\r\n", BoardCamera_SetPclk((uint8_t)us));
    else if (!strcmp(s,"camera shot")) VendorLog("shot accepted=%u (RAM only)\r\n",BoardCamera_Snapshot()==HAL_OK);
    else if (!strcmp(s,"camera live")) BoardCamera_Resume();
    else if (!strcmp(s,"camera rotate")) {const BoardCameraStatus *c=BoardCamera_GetStatus();BoardCamera_SetTransform(c->rotation+1U,c->mirror,c->flip);}
    else if (!strcmp(s,"camera mirror")) {const BoardCameraStatus *c=BoardCamera_GetStatus();BoardCamera_SetTransform(c->rotation,!c->mirror,c->flip);}
    else if (!strcmp(s,"camera flip")) {const BoardCameraStatus *c=BoardCamera_GetStatus();BoardCamera_SetTransform(c->rotation,c->mirror,!c->flip);}
    else if (!strcmp(s,"camera edge on")) BoardCamera_SetEdges(1);
    else if (!strcmp(s,"camera edge off")) BoardCamera_SetEdges(0);
    else if (!strcmp(s,"lcd rotate")) {
#if BOARD_ENABLE_UI
        UiService_SetRotation(UiService_Rotation()+1U);
#endif
    }
    else if (!strcmp(s, "camera off")) BoardCamera_Enable(0);
    else if (!strcmp(s, "camera on")) {
#if BOARD_ENABLE_UI
        UiService_LcdTest(0);
#endif
        BoardCamera_Enable(1);
    }
    else if (!strcmp(s, "camera retry")) { VisionLink_Enable(0); BoardCamera_Enable(0); VendorLog("camera init=%d\r\n", BoardCamera_Init()); }
    else if (!strcmp(s, "lcd test")) Integration_LcdTest();
    else if (!strcmp(s, "sd retry")) VendorLog("SD mount=%d\r\n", BoardStorage_Remount());
    else if (!strcmp(s, "sd read")) VendorLog("SD sector0 read=%d\r\n", BoardStorage_SdReadTest());
    else if (!strcmp(s, "sd filetest")) VendorLog("FatFs filetest=%d (BUSY=existing file preserved)\r\n", BoardStorage_FileTest());
    else if (!strcmp(s, "qspi test")) VendorLog("QSPI last-sector test=%d\r\n", BoardStorage_QspiTest());
    else if (!strcmp(s, "qspi map")) VendorLog("QSPI map=%d\r\n", BoardStorage_QspiMap(1));
    else if (!strcmp(s, "qspi unmap")) VendorLog("QSPI unmap=%d\r\n", BoardStorage_QspiMap(0));
    else if (sscanf(s, "pwm2 %u", &us) == 1 && us >= 500 && us <= 2500) VendorLog("PWM2 PD13=%u result=%d\r\n", us, BoardPwm_SetChannelUs(1,(uint16_t)us));
    else if (sscanf(s, "pwm angle %u %u", &y, &us) == 2 && y < 2 && us <= 180) VendorLog("PWM angle=%d\r\n", PwmServoControl_SetAngle((PwmServoId_t)y,(float)us));
    else if (!strcmp(s, "pwm off")) BoardPwm_Stop();
    else if (sscanf(s, "pwm %u", &us) == 1 && us >= 500 && us <= 2500) VendorLog("PWM1 PD12=%u result=%d\r\n", us, BoardPwm_SetUs((uint16_t)us));
    else if (!strcmp(s, "rtc")) {
        result = BoardRtc_Get(&t, &date);
        if (result == HAL_OK) VendorLog("RTC %u-%02u-%02u %02u:%02u:%02u\r\n", 2000U+date.Year, date.Month, date.Date, t.Hours, t.Minutes, t.Seconds);
        else VendorLog("RTC unavailable\r\n");
    } else if (sscanf(s, "rtc set %u %u %u %u %u %u", &y,&m,&d,&h,&minute,&second) == 6) {
        if (y > 2099 || m > 12 || d > 31 || h > 23 || minute > 59 || second > 59) result = HAL_ERROR;
        else result = BoardRtc_Set((uint16_t)y,(uint8_t)m,(uint8_t)d,(uint8_t)h,(uint8_t)minute,(uint8_t)second);
        VendorLog("RTC set=%d\r\n", result);
    }
#if BOARD_CAMERA_SENSOR == 5640
    else if (!strcmp(s, "af")) {
        if (!BoardCamera_GetStatus()->ready) VendorLog("camera unavailable\r\n");
        else { BoardCamera_Enable(0); result = OV5640_AF_Download_Firmware() == 0 ? HAL_OK : HAL_ERROR;
            if (result == HAL_OK) OV5640_AF_Trigger_Constant(); BoardCamera_Enable(1); VendorLog("AF=%d\r\n", result); }
    }
#endif
    else VendorLog("unknown command; @help\r\n");
}

void IntegrationConsole_Task(void)
{
    uint8_t channel; char line[LINE_SIZE];
    for (channel = 0; channel < 2; channel++) {
        ConsoleLine *c = &s_input[channel];
        if (!c->ready) continue;
        __DMB(); memcpy(line, c->line, sizeof(line));
        /* Clear mailbox only after copying. An ongoing discarded line stays discarded. */
        __DMB(); c->ready = 0;
        Execute(line,channel);
    }
    if((int32_t)(HAL_GetTick()-next_print)>=0){next_print=HAL_GetTick()+25U;if(help_group!=255)HelpTask();else ParamCommandTask();}
}
