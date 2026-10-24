from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[1]
hal='''#ifndef H
#define H
#include <stdint.h>
#define __no_init
#define __DMB() ((void)0)
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef struct {uint32_t ErrorCode;} DCMI_HandleTypeDef;
typedef struct {uint32_t State,ErrorCode; struct {uint32_t Mode;} Init;} DMA_HandleTypeDef;
extern uint32_t remaining;
#define __HAL_DMA_GET_COUNTER(x) remaining
#define HAL_DMA_STATE_READY 0
#define HAL_DMA_ERROR_NONE 0
#define HAL_DCMI_ERROR_NONE 0
#define DMA_NORMAL 0
#define DCMI_MODE_SNAPSHOT 0
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*);
HAL_StatusTypeDef HAL_DCMI_Stop(DCMI_HandleTypeDef*);
HAL_StatusTypeDef HAL_DCMI_Start_DMA(DCMI_HandleTypeDef*,uint32_t,uint32_t,uint32_t);
void HAL_DCMI_IRQHandler(DCMI_HandleTypeDef*);void HAL_DMA_IRQHandler(DMA_HandleTypeDef*);
#endif
'''
test='''#include <assert.h>
#include <stdio.h>
#include "board_camera.h"
DCMI_HandleTypeDef hdcmi;DMA_HandleTypeDef DMA_Handle_dcmi;
void HAL_Delay(uint32_t t){(void)t;}
void SCCB_WriteReg_16Bit(uint16_t r,uint8_t v){(void)r;(void)v;}
uint32_t SCCB_ErrorCount,remaining,draws,stops,min_y=0,max_y=240;uint8_t OV5640_FrameState;
static uint8_t verify_pixels,rr,mm,ff;
int DCMI_OV5640_Init(void){return 0;}
uint8_t SCCB_ReadReg_16Bit(uint16_t r){switch(r){case 0x4300:return 0x6f;case 0x501f:return 1;case 0x460c:return 0x22;case 0x3824:return 4;}return 0;}
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*d){d->ErrorCode=0;return HAL_OK;}
HAL_StatusTypeDef HAL_DCMI_Start_DMA(DCMI_HandleTypeDef*d,uint32_t mode,uint32_t ptr,uint32_t words){(void)d;(void)mode;(void)ptr;assert(words==28800);remaining=words;DMA_Handle_dcmi.State=1;return HAL_OK;}
HAL_StatusTypeDef HAL_DCMI_Stop(DCMI_HandleTypeDef*d){(void)d;stops++;return HAL_OK;}
void OV5640_DCMI_Stop(void){stops++;}
void HAL_DCMI_IRQHandler(DCMI_HandleTypeDef*d){(void)d;}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef*d){(void)d;}
void LCD_CopyBuffer(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t*p){(void)x;assert(w==240&&h==4&&y>=min_y&&y+h<=max_y);draws++;
 if(verify_pixels)for(unsigned j=0;j<h;j++)for(unsigned i=0;i<w;i++){
  unsigned dx=mm?239-i:i,dy=ff?239-(y+j):y+j,sx=dx,sy=dy;
  if(rr==1){sx=dy;sy=239-dx;}else if(rr==2){sx=239-dx;sy=239-dy;}else if(rr==3){sx=239-dy;sy=dx;}
  assert(p[j*w+i]==(uint16_t)(sy*240+sx));
 }
}
int main(void){
 const BoardCameraStatus*s=BoardCamera_GetStatus();assert(BoardCamera_Init()==HAL_OK);BoardCamera_SetPreview(1,0,240);
 BoardCamera_Task(0);assert(s->capturing);
 OV5640_FrameState=1;DMA_Handle_dcmi.State=0;remaining=0;hdcmi.ErrorCode=1;
 BoardCamera_Task(1);assert(s->frames==0&&s->timeouts==1&&!s->displaying&&s->last_dcmi_error==1);
 BoardCamera_Task(200);remaining=1;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;
 BoardCamera_Task(201);assert(!s->displaying);BoardCamera_Task(701);assert(s->timeouts==2&&s->remaining_words==1);
 BoardCamera_Task(900);remaining=0;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;
 BoardCamera_Task(901);assert(s->frames==1&&s->displaying);
 for(int i=0;i<60;i++)BoardCamera_Task(902+i);assert(draws==60&&!s->displaying);
 BoardCamera_Task(1200);assert(s->capturing);BoardCamera_Enable(0);assert(!s->capturing&&!s->enabled&&stops>0);

 BoardCamera_SetPreview(0,32,144);BoardCamera_Enable(1);BoardCamera_Task(1400);remaining=0;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;BoardCamera_Task(1401);
 assert(s->frames==2&&!s->displaying&&draws==60);
 min_y=32;max_y=176;BoardCamera_SetPreview(1,32,144);BoardCamera_Task(1600);remaining=0;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;BoardCamera_Task(1601);
 assert(s->displaying);for(int i=0;i<36;i++)BoardCamera_Task(1602+i);assert(draws==96&&!s->displaying);
 BoardCamera_Task(1800);remaining=0;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;BoardCamera_Task(1801);BoardCamera_Task(1802);assert(draws==97);
 BoardCamera_SetPreview(0,32,144);BoardCamera_Task(1803);assert(!s->displaying&&draws==97);
 BoardCamera_SetPreview(1,239,10);assert(!s->displaying);BoardCamera_Enable(0);
 min_y=0;max_y=240;for(unsigned i=0;i<57600;i++)BoardCamera_Frame()[i]=(uint16_t)i;
 BoardCamera_SetPreview(1,0,240);assert(BoardCamera_Snapshot()==HAL_OK);BoardCamera_Task(3000);
 remaining=0;OV5640_FrameState=1;DMA_Handle_dcmi.State=0;BoardCamera_Task(3001);assert(s->frozen&&s->frame_valid&&!s->capturing);
 verify_pixels=1;
 for(rr=0;rr<4;rr++)for(mm=0;mm<2;mm++)for(ff=0;ff<2;ff++){
  BoardCamera_SetTransform(rr,mm,ff);for(unsigned i=0;i<60;i++)BoardCamera_Task(3100+i);
  assert(!s->capturing&&!s->displaying&&s->frozen);
 }
 verify_pixels=0;assert(BoardCamera_Frame()[12345]==12345);
 BoardCamera_SetEdges(1);assert(s->edges);for(unsigned i=0;i<60;i++)BoardCamera_Task(3300+i);
 BoardCamera_Resume();BoardCamera_Task(4000);assert(s->capturing&&!s->frozen);BoardCamera_Enable(0);
 puts("PASS transform: all 16 rotate/mirror/flip combinations over full frame; immutable raw capture; snapshot holds frame; resume restarts capture; edge path executes");
 puts("PASS CAMERA: reject erroneous and incomplete DMA frames, accept full frame, strip display, stop capture");
 puts("PASS VIEWPORT: capture without LCD writes, bounded 240x144 preview at y32, cancel partial preview on navigation, invalid-window guard");
{uint32_t seq,tick;BoardCamera_Resume();BoardCamera_Enable(0);
 const uint16_t *lease=BoardCamera_AcquireFrame(&seq,&tick);assert(lease&&seq==s->frames&&tick==s->frame_tick);
 BoardCamera_Enable(1);BoardCamera_SetPreview(0,0,240);BoardCamera_Task(100000);assert(!s->capturing);
 assert(!BoardCamera_AcquireFrame(&seq,&tick));BoardCamera_ReleaseFrame();BoardCamera_Task(100001);assert(s->capturing);
 BoardCamera_Enable(0);puts("PASS frame lease: capture paused until release; double acquire rejected");}
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'platform_hal.h').write_text(hal);(p/'stm32h7xx_hal.h').write_text(hal);(p/'dcmi_ov5640.h').write_text('#include "platform_hal.h"\nextern uint32_t SCCB_ErrorCount;extern uint8_t OV5640_FrameState;\nvoid SCCB_WriteReg_16Bit(uint16_t,uint8_t);int DCMI_OV5640_Init(void);void OV5640_DCMI_Stop(void);uint8_t SCCB_ReadReg_16Bit(uint16_t);\n');(p/'lcd_spi_154.h').write_text('#include "platform_hal.h"\nvoid LCD_CopyBuffer(uint16_t,uint16_t,uint16_t,uint16_t,uint16_t*);\n');(p/'test.c').write_text(test)
 cmd=[r'C:\Dev-Cpp\MinGW64\bin\gcc.exe','-DSTM32H743xx','-std=c99','-Wall','-Wextra','-Werror','-Wno-unknown-pragmas','-Wno-pointer-to-int-cast','-Wno-misleading-indentation','-I'+d,'-I'+str(root/'project/App/platform'),'-I'+str(root/'project/App/config'),str(p/'test.c'),str(root/'project/App/platform/board_camera.c'),'-o',str(p/'test.exe')]
 subprocess.run(cmd,check=True)
 run=subprocess.run([str(p/'test.exe')],capture_output=True,text=True)
 result={'scope':'host-only actual board_camera.c with DMA/DCMI fault model','exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
 (root/'Docs/故障排查/20261021_花屏与舵机离线/摄像头主机测试.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
 print(run.stdout,end='');run.check_returncode()
