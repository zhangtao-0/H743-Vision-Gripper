"""Actual JustFloat encoder and HOST queue isolation; no real motor or serial IO."""
from pathlib import Path
import subprocess,tempfile,runpy,json,struct,ctypes
H=Path(__file__).resolve().parents[1]
HAL=runpy.run_path(str(H/'Tools/run_p3_host_tests.py'))['HAL']
HAL='#define __get_PRIMASK() 0U\n#define __set_PRIMASK(p) ((void)(p))\n'+HAL
HAL=HAL.replace('#endif','HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);\n#endif')
TEST=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "vofa_protocol.h"
#include "motor_plot.h"
#include "motor_service.h"
static uint32_t tick;static uint8_t output[8192];static unsigned length;static MotorStatus state;
UART_HandleTypeDef huart1,huart2,huart3;
uint32_t HAL_GetTick(void){return tick;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*u){(void)u;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*u){(void)u;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*u,const uint8_t*p,uint16_t n,uint32_t t){(void)u;(void)p;(void)n;(void)t;return HAL_ERROR;}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){assert(u==&huart1&&length+n<sizeof(output));memcpy(output+length,p,n);length+=n;return HAL_OK;}
uint8_t FeetechSts_IsValidFrame(const uint8_t*p,uint8_t n){(void)p;(void)n;return 0;}
uint8_t FeetechSts_GetFrameTotalLen(const uint8_t*p,uint8_t n){(void)p;(void)n;return 0;}
uint8_t ClawHostProtocol_IsFrame(const uint8_t*p,uint8_t n){(void)p;(void)n;return 0;}
uint8_t IntegrationConsole_PushByte(uint8_t b,uint8_t c,uint8_t a){(void)b;(void)c;(void)a;return 1;}
void MotorService_Snapshot(MotorStatus*p){*p=state;}
int VendorLog(const char*f,...){(void)f;return 0;}
static void drain(void){while(VOFA_Protocol_GetStatus()->tx_busy)VOFA_Protocol_TxCpltCallback(&huart1);}
static float get(unsigned offset){float v;memcpy(&v,output+offset,4);return v;}
int main(void){
 VOFA_Protocol_Init();MotorPlot_Init();MotorPlot_Task(0);assert(!length&&!MotorPlot_IsEnabled());
 assert(VOFA_Protocol_SendRaw((const uint8_t*)"old",3)==HAL_OK);assert(!MotorPlot_Enable(1));drain();
 assert(MotorPlot_Enable(1));assert(VOFA_Protocol_SendRaw((const uint8_t*)"text",4)==HAL_BUSY);
 length=0;state.control.target_rpm=400;state.control.requested_direction=-1;state.control.active_direction=-1;
 state.control.measured_rpm=-248.75f;state.control.ramped_rpm=250;state.control.raw_rpm=-260;state.control.pwm=-612;state.control.enabled=1;
 MotorPlot_Task(0);assert(length==36);assert(get(0)==-400 && get(4)==-248.75f && get(8)==-250);
 assert(get(12)==-260 && get(16)==-612 && get(20)==-1.25f && get(24)==1 && get(28)==0);
 assert(!memcmp(output+32,"\x00\x00\x80\x7f",4));
 printf("FRAME|");for(unsigned i=0;i<36;i++)printf("%02x",output[i]);puts("");drain();
 MotorPlot_Task(19);assert(length==36);MotorPlot_Task(20);assert(length==72);drain();
 state.control.enabled=0;state.control.ramped_rpm=250;MotorPlot_Task(40);assert(get(72+8)==0);drain();
 assert(MotorPlot_Enable(0));tick=0xfffffff0U;assert(MotorPlot_Enable(1));MotorPlot_Task(tick);drain();unsigned before=length;
 MotorPlot_Task(3);assert(length==before);MotorPlot_Task(4);assert(length==before+36);drain();
 for(unsigned i=0;i<100;i++)MotorPlot_Task(24+i*20);assert(VOFA_Protocol_GetStatus()->tx_overflow_count>0);
 assert(MotorPlot_Enable(0));drain();assert(VOFA_Protocol_SendPlot(output,36)==HAL_ERROR);
 assert(VOFA_Protocol_SendRaw((const uint8_t*)"ok",2)==HAL_OK);drain();
 puts("PASS plot: disabled startup, exact 8-channel signed JustFloat, 50Hz, wrap, inactive reference zero, backpressure, exclusive HOST and restored text");
 return 0;
}
'''
def main():
 ctypes.windll.kernel32.SetErrorMode(0x8003)
 with tempfile.TemporaryDirectory(prefix='h743-plot-') as folder:
  p=Path(folder);(p/'stm32h7xx_hal.h').write_text(HAL);(p/'test.c').write_text(TEST)
  app=H/'project/App';cmd=[r'C:\Dev-Cpp\MinGW64\bin\gcc.exe','-DSTM32H743xx','-DPLATFORM_TEST_UART_BYTE_ONLY=1','-std=c99','-Wall','-Wextra','-Werror']
  for rel in ['', 'config','platform','protocol','drivers/Servo','modules/claw_core','modules/integration','modules/motor','platform/boards/lxb743zi_p1']:
   cmd+=['-I',str(p if not rel else app/rel)]
  cmd += [str(p/'test.c'),str(app/'platform/board_resources.c'),str(app/'platform/board_uart.c'),str(app/'protocol/vofa_protocol.c'),str(app/'modules/motor/motor_plot.c'),'-o',str(p/'test.exe')]
  build=subprocess.run(cmd,capture_output=True,text=True)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(p/'test.exe')],capture_output=True,text=True,timeout=15)
  if run.returncode:raise SystemExit(run.stdout+run.stderr)
  frame=next(line[6:] for line in run.stdout.splitlines() if line.startswith('FRAME|'))
  assert struct.unpack('<8f',bytes.fromhex(frame)[:32])==(-400,-248.75,-250,-260,-612,-1.25,1,0)
  report=dict(scope='actual C telemetry and UART queue, mocked UART/motor state; not physical motor data',exit_code=run.returncode,stdout=run.stdout,python_cross_decode=True)
  (H/'Docs/电机控制/20261023_VOFA调速与波形/JustFloat主机测试.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(run.stdout)
if __name__=='__main__':main()
