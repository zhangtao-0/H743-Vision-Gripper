"""Compile the actual PID, EC11 decoder and service against a deterministic host plant/board model."""
from pathlib import Path
import subprocess, tempfile, json
ROOT=Path(__file__).resolve().parents[1]
GCC=r"C:\Dev-Cpp\MinGW64\bin\gcc.exe"
HAL=r'''
#ifndef H
#define H
#include <stdint.h>
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
extern uint32_t primask;
#define __get_PRIMASK() primask
#define __disable_irq() (primask=1)
#define __set_PRIMASK(p) (primask=(p))
uint32_t HAL_GetTick(void);
#endif
'''
TEST=r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "motor_control.h"
#include "motor_service.h"
#include "board_motor.h"
#include "input_control.h"
static InputMode input_mode;static uint8_t virtual_button;
static uint8_t grant=1;
uint8_t Input_DebugEnabled(uint8_t target){return target==1&&grant;}
InputMode Input_GetMode(void){return input_mode;}
uint8_t Input_Resolve(uint8_t key,uint8_t p,uint32_t now){(void)key;(void)now;return (input_mode!=INPUT_REMOTE&&p)||(input_mode!=INPUT_PHYSICAL&&virtual_button);}
static uint32_t tick;uint32_t primask;
static uint16_t counter;static uint8_t button,ab=3,driver=1;
static int16_t output;static float peak;
uint32_t HAL_GetTick(void){return tick;}
HAL_StatusTypeDef BoardMotor_Init(void){return HAL_OK;}
void BoardMotor_Write(int16_t p){output=p;}
uint16_t BoardMotor_Count(void){return counter;}
uint8_t BoardMotor_Knob(void){return ab;}
uint8_t BoardMotor_Button(void){return button;}
uint8_t BoardMotor_DriverAvailable(void){return driver;}
int VendorLog(const char *f,...){(void)f;return 0;}
static void ticks(int n){for(int i=0;i<n;i++){tick++;MotorService_Tick1ms();}}
static void Plant(MotorControl *m,float *speed,float *fraction,float load,int steps){
 for(int i=0;i<steps;i++){
  float target=m->pwm*0.5f;
  if(target>0)target-=load;else if(target<0)target+=load;
  *speed+=0.01f/0.18f*(target-*speed);
  *fraction+=*speed*m->config.counts_per_rev/6000.0f;
  int32_t delta=(int32_t)*fraction;*fraction-=delta;
  MotorControl_Step(m,delta);
  if(fabsf(m->integral)>peak)peak=fabsf(m->integral);
  assert(abs(m->pwm)<=950);
 }
}
int main(void){
 MotorConfig cfg={1.8f,5,0,1061.268f,400,150,950,0.25f};MotorControl m;
 MotorControl_Init(&m,&cfg);assert(!m.enabled && !m.pwm);
 assert(!MotorControl_SetTarget(&m,401));assert(!MotorControl_SetTarget(&m,NAN));
 assert(MotorControl_Delta16(2,65534)==4);assert(MotorControl_Delta16(65534,2)==-4);
 cfg.counts_per_rev=0;MotorControl_Init(&m,&cfg);assert(!MotorControl_Enable(&m,1));cfg.counts_per_rev=1061.268f;
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,200);
 float speed=0,fraction=0;Plant(&m,&speed,&fraction,0,1200);assert(!m.fault && fabsf(speed-200)<3);
 Plant(&m,&speed,&fraction,70,1000);assert(!m.fault && fabsf(speed-200)<3);
 printf("PASS PID plant: target=200 RPM, 70 RPM equivalent load disturbance, final=%.2f RPM\n",speed);
 MotorControl_Reverse(&m);MotorControl_Step(&m,35);assert(m.reversing && m.pwm==0);
 for(int i=0;i<9;i++){MotorControl_Step(&m,0);assert(m.pwm==0);}
 MotorControl_Step(&m,0);assert(!m.reversing && m.active_direction==-1 && !m.pwm);
 speed=0;fraction=0;Plant(&m,&speed,&fraction,0,1200);assert(!m.fault&&fabsf(speed+200)<3);
 MotorControl_Stop(&m);assert(!m.pwm && !m.enabled && !m.target_rpm);
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,200);
 for(int i=0;i<500;i++)MotorControl_Step(&m,0);assert(m.fault==MOTOR_FAULT_NO_FEEDBACK && !m.pwm);
 MotorControl_ClearFault(&m);assert(!m.enabled && !m.fault);
 MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,200);
 for(int i=0;i<500 && !m.fault;i++)MotorControl_Step(&m,-20);
 assert(m.fault==MOTOR_FAULT_FEEDBACK_SIGN && !m.pwm);
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_Reverse(&m);
 for(int i=0;i<301;i++)MotorControl_Step(&m,20);assert(m.fault==MOTOR_FAULT_REVERSE_TIMEOUT);
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,400);
 for(int i=0;i<1000;i++)MotorControl_Step(&m,18);
 assert(!m.fault && m.pwm>=900 && m.integral>0 && m.integral<700);
 assert(peak<=950 && m.integral<=950);MotorControl_SetTarget(&m,0);MotorControl_Step(&m,0);assert(m.pwm==0&&m.integral==0);
 cfg.kd=0.01f;MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,100);
 speed=0;fraction=0;Plant(&m,&speed,&fraction,0,1200);assert(!m.fault&&fabsf(speed-100)<3);
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,100);
 for(int i=0;i<10;i++)MotorControl_Step(&m,200);assert(m.fault==MOTOR_FAULT_OVERSPEED&&!m.enabled&&!m.pwm);
 MotorControl_ClearFault(&m);assert(!m.enabled&&!m.target_rpm&&!m.fault);
 MotorControl_Init(&m,&cfg);MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,400);
 for(int i=0;i<150&&!m.fault;i++)MotorControl_Step(&m,i%20==0?1:0);
 assert(m.fault==MOTOR_FAULT_NO_FEEDBACK&&!m.enabled&&!m.pwm);
 MotorControl_ClearFault(&m);assert(!m.enabled&&!m.feedback_progress);
 MotorControl_Enable(&m,1);MotorControl_SetTarget(&m,400);
 for(int i=0;i<150&&!m.fault;i++)MotorControl_Step(&m,i%2?1:-1);
 assert(m.fault==MOTOR_FAULT_NO_FEEDBACK&&!m.enabled&&!m.pwm);
 puts("PASS sparse feedback: isolated pulses and alternating jitter cannot bypass300ms effort/progress guard");
 puts("PASS overspeed: sustained implausible encoder speed disables output; clear never restarts");
 puts("PASS control: ramp, signed feedback, reverse coast/deadtime, timeout/sign faults, anti-windup, zero-stop");
 MotorKnob k={3,0,4};int sum=0;uint8_t seq[]={1,3,1,0,2,3};
 for(unsigned i=0;i<sizeof(seq);i++)sum+=MotorKnob_Sample(&k,seq[i]);assert(sum==1);
 assert(MotorKnob_Sample(&k,0)==0 && k.quarters==0);
 MotorKnob k2={3,0,2};assert(!MotorKnob_Sample(&k2,1));assert(MotorKnob_Sample(&k2,0)==1);puts("PASS EC11: one detent, contact bounce cancellation, invalid transition rejection");
 driver=0;MotorService_Init();MotorStatus initial;MotorService_Snapshot(&initial);assert(initial.control.target_rpm==400&&!initial.control.enabled&&!initial.control.pwm);assert(!MotorService_Enable(1));assert(output==0);
 driver=1;MotorService_Init();assert(MotorService_SetTarget(100));assert(MotorService_Enable(1));
 ticks(100);assert(output>0);tick+=100;ticks(1);MotorStatus st;MotorService_Snapshot(&st);
 assert(st.control.fault==MOTOR_FAULT_TIMING && !output);
 MotorService_ClearFault();assert(MotorService_ZeroCount());
 button=1;ticks(60);button=0;ticks(30);MotorService_Snapshot(&st);assert(st.control.enabled);
 assert(!MotorService_SetCpr(1040));assert(!MotorService_SetPid(1000,1000,0));
 button=1;ticks(1040);MotorService_Snapshot(&st);assert(st.control.requested_direction==-1);
 button=0;ticks(30);MotorService_Snapshot(&st);assert(st.control.enabled); /* long release does not toggle */
 button=1;ticks(60);button=0;ticks(30);MotorService_Snapshot(&st);assert(!st.control.enabled);
 assert(MotorService_SetTarget(225));assert(MotorService_Enable(1));assert(MotorService_Enable(0));MotorService_Snapshot(&st);assert(st.control.target_rpm==225&&!st.control.enabled&&!output);
 MotorService_Stop();assert(output==0);assert(MotorService_SetCpr(1061));
 primask=1;MotorService_Snapshot(&st);assert(primask==1);primask=0;
 puts("PASS service: missing-driver inhibit, IRQ timing fault, button debounce/long vs short, runtime-setting guards, preserve PRIMASK");
 assert(MotorService_BeginMaintenance());assert(!MotorService_Enable(1));assert(!MotorService_BeginMaintenance());
 assert(MotorService_ApplyParameters(1061.268f,123,2100,3500,0));MotorService_Snapshot(&st);
 assert(!st.control.enabled && st.control.target_rpm==123 && fabsf(st.control.config.counts_per_rev-1061.268f)<0.01f);
 assert(!MotorService_ApplyParameters(NAN,0,0,0,0));assert(!MotorService_ApplyParameters(1000,401,0,0,0));
 MotorService_EndMaintenance();assert(MotorService_Enable(1));assert(!MotorService_BeginMaintenance());assert(!MotorService_ResetParameters());MotorService_Stop();
 assert(MotorService_ResetParameters());MotorService_Snapshot(&st);assert(!st.control.enabled && st.control.target_rpm==400 && fabsf(st.control.config.kp-1.8f)<0.01f);
 grant=0;assert(!MotorService_SimulateKnob(-2));button=1;ticks(60);button=0;ticks(30);MotorService_Snapshot(&st);assert(!st.control.enabled);grant=1;
 assert(MotorService_SimulateKnob(-2));MotorService_Snapshot(&st);assert(st.control.target_rpm==390);
 input_mode=INPUT_PHYSICAL;assert(!MotorService_SimulateKnob(1));input_mode=INPUT_REMOTE;
 int8_t old_direction=st.control.requested_direction;virtual_button=1;ticks(100);virtual_button=0;ticks(40);MotorService_Snapshot(&st);assert(st.control.requested_direction==old_direction && st.control.enabled);
 assert(MotorService_SetTarget(0));virtual_button=1;ticks(1040);virtual_button=0;ticks(40);MotorService_Snapshot(&st);assert(st.control.requested_direction==-old_direction&&st.control.enabled);
 assert(!MotorService_SimulateKnob(81));input_mode=INPUT_MIXED;
 puts("PASS virtual motor: shared detent limits, physical-only reject, simulated button uses debounce/direction gesture");
 puts("PASS persistence gates: maintenance inhibits starts, atomic settings restore, finite/range checks, RAM defaults stay disabled");

 return 0;
}
'''
def main():
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'platform_hal.h').write_text(HAL);(p/'stm32h7xx_hal.h').write_text(HAL);(p/'test.c').write_text(TEST.replace('#include <assert.h>','#include <assert.h>\n#include <stdlib.h>'))
  # board_motor.h is used directly; only its HAL provider is replaced.
  app=ROOT/'project/App'
  command=[GCC,'-DSTM32H743xx','-std=c99','-Wall','-Wextra','-Werror','-O0']
  for inc in [p,app/'bsp',app/'modules/motor',app/'platform',app/'config',app/'modules/integration']:command+=['-I',str(inc)]
  command +=[str(p/'test.c'),str(app/'modules/motor/motor_control.c'),str(app/'modules/motor/motor_service.c'),'-o',str(p/'test.exe'),'-lm']
  build=subprocess.run(command,capture_output=True,text=True)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(p/'test.exe')],capture_output=True,text=True)
  result={'scope':'host-only actual motor PID/service; simulated first-order motor plant, no real H-bridge or motor test','exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
  (ROOT/'Docs/电机控制/主机测试.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print(run.stdout,run.stderr,end='');run.check_returncode()
if __name__=='__main__':main()
