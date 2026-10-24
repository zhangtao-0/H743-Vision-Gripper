/* Opt-in 50Hz HOST JustFloat telemetry, independent from the 10ms PID ISR. */
#include "motor_plot.h"
#include "motor_service.h"
#include "vofa_protocol.h"
#include "vendor_log.h"
#include <string.h>
typedef char FloatMustBe32Bits[(sizeof(float)==4U)?1:-1];
static uint8_t enabled;
static uint32_t next_tick,sent,dropped;
uint8_t MotorPlot_IsEnabled(void){return enabled;}
void MotorPlot_Init(void){enabled=0;next_tick=sent=dropped=0;(void)VOFA_Protocol_SetPlotExclusive(0);}
uint8_t MotorPlot_Enable(uint8_t on){
 if(!VOFA_Protocol_SetPlotExclusive(on))return 0;
 enabled=(on!=0);next_tick=HAL_GetTick();return 1;
}
void MotorPlot_Task(uint32_t now){
 MotorStatus s;float values[8];uint8_t frame[36];uint32_t bits;uint8_t i;
 if(!enabled || (int32_t)(now-next_tick)<0)return;
 next_tick=now+20U;MotorService_Snapshot(&s);
 values[0]=s.control.target_rpm*s.control.requested_direction;
 values[1]=s.control.measured_rpm;
 values[2]=(s.control.enabled && !s.control.reversing)?s.control.ramped_rpm*s.control.active_direction:0;
 values[3]=s.control.raw_rpm;values[4]=(float)s.control.pwm;
 values[5]=values[2]-values[1];values[6]=(float)s.control.enabled;values[7]=(float)s.control.fault;
 for(i=0;i<8;i++){
  memcpy(&bits,&values[i],4);frame[4*i]=(uint8_t)bits;frame[4*i+1]=(uint8_t)(bits>>8);
  frame[4*i+2]=(uint8_t)(bits>>16);frame[4*i+3]=(uint8_t)(bits>>24);
 }
 frame[32]=0;frame[33]=0;frame[34]=0x80;frame[35]=0x7f;
 if(VOFA_Protocol_SendPlot(frame,sizeof(frame))==HAL_OK)sent++;else dropped++;
}
void MotorPlot_Report(void){VendorLog("MOTORPLOT on=%u hz=50 channels=8 sent=%lu dropped=%lu HOST=JustFloat USB=text\r\n",enabled,(unsigned long)sent,(unsigned long)dropped);}
