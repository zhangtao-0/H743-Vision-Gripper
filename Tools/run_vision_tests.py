"""Actual C USB/vision state tests plus cross-language wire verification; no serial IO."""
from pathlib import Path
import subprocess,tempfile,json,sys
H=Path(__file__).resolve().parents[1]
P=H/'project/App/platform'
GCC=r'C:\Dev-Cpp\MinGW64\bin\gcc.exe'
HAL="""#ifndef H
#define H
#include <stdint.h>
#define __DMB() ((void)0)
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
uint32_t HAL_GetTick(void);
#endif
"""
VISION=r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vision_link.h"
#include "board_camera.h"
#include "video_wire.h"
static uint8_t pixels[240*240*2],header[40],busy,connected=1,leased;
static uint32_t now,started,aborts;
static BoardCameraStatus camera={0};
uint32_t HAL_GetTick(void){return now;}
const BoardCameraStatus *BoardCamera_GetStatus(void){return &camera;}
const uint16_t *BoardCamera_AcquireFrame(uint32_t *seq,uint32_t *tick){assert(!leased);leased=1;*seq=camera.frames;*tick=camera.frame_tick;return (const uint16_t*)pixels;}
void BoardCamera_ReleaseFrame(void){assert(leased);leased=0;}
uint8_t UsbConsole_Connected(void){return connected;}
uint8_t UsbConsole_StreamBusy(void){return busy;}
void UsbConsole_StreamAbort(void){busy=0;aborts++;}
uint8_t UsbConsole_StreamStart(const uint8_t*h,uint16_t n,const uint8_t*p,uint32_t size){assert(leased&&n==40&&size==sizeof(pixels)&&p==pixels);memcpy(header,h,n);busy=1;started++;return 1;}
int VendorLog(const char *format,...){(void)format;return 0;}
static void complete_frame(uint32_t at,uint32_t seq){
 camera.frames=seq;camera.frame_tick=at;camera.frame_valid=1;
 for(now=at;now<at+100;now++)VisionLink_Task(now);
 assert(busy&&leased);busy=0;VisionLink_Task(now++);assert(!leased);
}
int main(int argc,char**argv){
 uint32_t crc;FILE*f;assert(argc==2);
 assert(!VisionLink_Enable(1));camera.ready=camera.enabled=1;assert(VisionLink_Enable(1));
 assert(!VisionLink_SetFps(0)&&!VisionLink_SetFps(6)&&VisionLink_SetFps(2));
 assert(!VisionLink_Accept(7,120,120,0,50,900));
 for(unsigned i=0;i<sizeof(pixels);i++)pixels[i]=(uint8_t)i;
 complete_frame(1000,7);assert(started==1);
 assert(VisionLink_Accept(7,120,119,-900,50,900));assert(VisionLink_Advice(now)->valid);
 assert(!VisionLink_Accept(6,120,120,0,50,900));assert(!VisionLink_Accept(7,-1,120,0,50,900));
 assert(!VisionLink_Accept(7,240,120,0,50,900));assert(!VisionLink_Accept(7,120,120,900,50,900));
 assert(!VisionLink_Accept(7,120,120,0,0,900));assert(!VisionLink_Accept(7,120,120,0,50,1001));
 f=fopen(argv[1],"wb");assert(f);fwrite(header,1,40,f);fwrite(pixels,1,sizeof(pixels),f);fclose(f);
 crc=VideoCrc(0xFFFFFFFFU,(const uint8_t*)"123456789",9)^0xFFFFFFFFU;assert(crc==0xCBF43926U);
 now=2501;assert(!VisionLink_Advice(now)->valid);assert(!VisionLink_Accept(7,120,120,0,50,900));
 VisionLink_Enable(0);VisionLink_Enable(1);assert(!VisionLink_Accept(7,120,120,0,50,900));
 complete_frame(3000,8);assert(VisionLink_Accept(8,120,120,0,50,900));
 connected=0;assert(!VisionLink_Advice(now)->valid);VisionLink_Task(now);assert(!leased);connected=1;
 camera.frames=9;camera.frame_tick=4000;for(now=4000;now<4200;now++)VisionLink_Task(now);
 assert(busy&&leased);now=8100;VisionLink_Task(now);assert(!busy&&!leased&&aborts);
 puts("PASS C vision: lease/CRC/frame completion, bounds, stale/session/disconnect rejection, stalled USB timeout; advisory-only");
 return 0;
}'''
USB=r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "usb_console.h"
#include "usbd_cdc_if.h"
USBD_CDC_HandleTypeDef cdc;
USBD_HandleTypeDef hUsbDeviceFS={1,&cdc};
static uint8_t out[2000],pending[64],*owned;static uint16_t owned_n;static unsigned length;
void MX_USB_DEVICE_Init(void){}
uint8_t IntegrationConsole_PushByte(uint8_t b,uint8_t channel,uint8_t allow){(void)b;(void)channel;(void)allow;return 1;}
uint8_t CDC_Transmit_FS(uint8_t*p,uint16_t n){assert(!cdc.TxState&&n<=64);owned=p;owned_n=n;memcpy(pending,p,n);cdc.TxState=1;return 0;}
static void ack(void){assert(cdc.TxState&&!memcmp(owned,pending,owned_n));memcpy(out+length,owned,owned_n);length+=owned_n;cdc.TxState=0;}
int main(void){
 uint8_t head[40],payload[150];memset(head,0x45,sizeof(head));memset(payload,0x79,sizeof(payload));
 UsbConsole_Write((const uint8_t*)"BEFORE",6);assert(!UsbConsole_StreamStart(head,40,payload,150));
 UsbConsole_Task();UsbConsole_Task();assert(length==0);ack();assert(length==6);
 assert(UsbConsole_StreamStart(head,40,payload,150));UsbConsole_Task();UsbConsole_Write((const uint8_t*)"AFTER",5);
 UsbConsole_Task();ack();while(UsbConsole_StreamBusy()){UsbConsole_Task();ack();}
 UsbConsole_Task();ack();assert(length==201&&!memcmp(out,"BEFORE",6)&&!memcmp(out+6,head,40)&&!memcmp(out+46,payload,150)&&!memcmp(out+196,"AFTER",5));
 assert(UsbConsole_StreamStart(head,40,payload,150));hUsbDeviceFS.dev_state=0;UsbConsole_Task();assert(!UsbConsole_StreamBusy());
 puts("PASS C USB: packet ownership until endpoint completion, exact payload/text separation, pending-text priority, disconnect cancellation");return 0;
}'''

def main():
 reports=[]
 with tempfile.TemporaryDirectory() as folder:
  t=Path(folder)
  (t/'stm32h7xx_hal.h').write_text(HAL)
  for name,body in {
   'vendor_log.h':'int VendorLog(const char*,...);',
   'usb_device.h':'void MX_USB_DEVICE_Init(void);',
   'integration_console.h':'#include <stdint.h>\nuint8_t IntegrationConsole_PushByte(uint8_t,uint8_t,uint8_t);',
   'usbd_cdc_if.h':'#include <stdint.h>\ntypedef struct {uint32_t TxState;} USBD_CDC_HandleTypeDef;\ntypedef struct {uint32_t dev_state;void *pClassData;} USBD_HandleTypeDef;\n#define USBD_STATE_CONFIGURED 1\n#define USBD_OK 0\nuint8_t CDC_Transmit_FS(uint8_t*,uint16_t);'
  }.items():(t/name).write_text(body)
  for name,source in [('vision',VISION),('usb',USB)]:
   test=t/(name+'.c');test.write_text(source)
   target=P/('vision_link.c' if name=='vision' else 'usb_console.c')
   exe=t/(name+'.exe')
   cmd=[GCC,'-DSTM32H743xx','-std=c99','-Wall','-Wextra','-Werror','-I'+str(t),'-I'+str(P),'-I'+str(H/'project/App/config'),str(test),str(target),'-o',str(exe)]
   subprocess.run(cmd,check=True)
   output=subprocess.run([str(exe),str(t/'wire.bin')],capture_output=True,text=True)
   reports.append(dict(test=name,exit_code=output.returncode,stdout=output.stdout,stderr=output.stderr));print(output.stdout,end='');output.check_returncode()
  sys.path.insert(0,str(H/'Host/vision_gripper'))
  from wire import Decoder
  frames=Decoder().feed((t/'wire.bin').read_bytes())
  assert len(frames)==1 and frames[0].sequence==7 and frames[0].pixels==bytes(range(256))*450
  print('PASS cross-language: actual C header/payload decoded and CRC-verified by Python')
 result=dict(scope='actual C USB/vision with endpoint/camera models; no physical IO',checks=reports,cross_language=True,passed=True)
 (H/'Docs/视觉识别与抓取/20261023_电脑视觉首版/固件链路主机测试.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
if __name__=='__main__':main()
