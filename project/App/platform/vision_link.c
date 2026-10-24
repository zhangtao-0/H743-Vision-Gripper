#include "vision_link.h"
#include "video_wire.h"
#include "board_camera.h"
#include "integration_config.h"
#include "usb_console.h"
#include "vendor_log.h"
#include <string.h>
/* At most 2 KiB CRC per loop, then USB endpoint packets; no blocking transmit. */
#define FRAME_BYTES (BOARD_CAMERA_WIDTH*BOARD_CAMERA_HEIGHT*2U)
#define VIDEO_TIMEOUT_MS 4000U
#define ADVICE_MAX_AGE_MS 1500U
static const uint8_t *s_frame;
static uint8_t s_header[VIDEO_HEADER_SIZE],s_enabled,s_stage,s_has_sent;
static uint32_t s_period=500U,s_next,s_started,s_offset,s_crc,s_sequence,s_tick,s_flags;
static uint32_t s_sent,s_aborted,s_last_sequence,s_last_tick;
static VisionAdvice s_advice;
static void Release(void){if(s_frame)BoardCamera_ReleaseFrame();s_frame=0;s_stage=0;}
uint8_t VisionLink_Enable(uint8_t enable){
 if(enable && (!BOARD_ENABLE_CAMERA || !BOARD_ENABLE_USB_CDC || !BoardCamera_GetStatus()->ready))return 0;
 if(!s_enabled && enable){s_has_sent=0;VisionLink_Clear();}
 s_enabled=(enable!=0);if(!s_enabled){UsbConsole_StreamAbort();Release();VisionLink_Clear();}
 return 1;
}
uint8_t VisionLink_SetFps(uint32_t fps){if(fps<1U||fps>5U)return 0;s_period=1000U/fps;return 1;}
void VisionLink_Clear(void){memset(&s_advice,0,sizeof(s_advice));}
const VisionAdvice *VisionLink_Advice(uint32_t now){
 if(s_advice.valid && ((uint32_t)(now-s_advice.frame_tick)>ADVICE_MAX_AGE_MS || !s_enabled || !UsbConsole_Connected() || !BoardCamera_GetStatus()->enabled))s_advice.valid=0;
 return &s_advice;
}
uint8_t VisionLink_Accept(uint32_t seq,int32_t u,int32_t v,int32_t angle10,int32_t width,int32_t score){
 uint32_t now=HAL_GetTick();
 if(!s_enabled||!s_has_sent||!UsbConsole_Connected()||!BoardCamera_GetStatus()->enabled||seq!=s_last_sequence||
 (uint32_t)(now-s_last_tick)>ADVICE_MAX_AGE_MS||u<0||(uint32_t)u>=BOARD_CAMERA_WIDTH||v<0||(uint32_t)v>=BOARD_CAMERA_HEIGHT||
 angle10< -900||angle10>=900||width<1||(uint32_t)width>BOARD_CAMERA_WIDTH||score<0||score>1000)return 0;
 s_advice.sequence=seq;s_advice.frame_tick=s_last_tick;s_advice.received_tick=now;
 s_advice.u=u;s_advice.v=v;s_advice.angle10=angle10;s_advice.width=width;s_advice.score=score;s_advice.valid=1;
 return 1; /* advisory only: never calls an actuator or changes control ownership */
}
void VisionLink_Report(void){
 const VisionAdvice *a=VisionLink_Advice(HAL_GetTick());
 VendorLog("VIDEO on=%u fpsMax=%lu sent=%lu abort=%lu busy=%u USB=%u\r\n",s_enabled,(unsigned long)(1000U/s_period),(unsigned long)s_sent,(unsigned long)s_aborted,s_stage,UsbConsole_Connected());
 VendorLog("VISION advisory=%u seq=%lu u=%ld v=%ld angle10=%ld widthPx=%ld score=%ld; NO MOTION\r\n",a->valid,(unsigned long)a->sequence,(long)a->u,(long)a->v,(long)a->angle10,(long)a->width,(long)a->score);
}
void VisionLink_Task(uint32_t now){
 const BoardCameraStatus *c=BoardCamera_GetStatus();uint32_t n;
 (void)VisionLink_Advice(now);
 if(!s_enabled)return;
 if(!UsbConsole_Connected() || !c->enabled || !c->ready){
  if(s_stage){UsbConsole_StreamAbort();Release();s_aborted++;}return;
 }
 if(s_stage && (uint32_t)(now-s_started)>VIDEO_TIMEOUT_MS){UsbConsole_StreamAbort();Release();s_aborted++;s_next=now+s_period;return;}
 if(s_stage==3){
  if(!UsbConsole_StreamBusy()){s_sent++;s_has_sent=1;s_last_sequence=s_sequence;s_last_tick=s_tick;Release();s_next=now+s_period;}
  return;
 }
 if(!s_stage){
  if((int32_t)(now-s_next)<0 || !c->frame_valid || (s_sent && c->frames==s_last_sequence))return;
  s_frame=(const uint8_t*)BoardCamera_AcquireFrame(&s_sequence,&s_tick);
  if(!s_frame)return;
  s_flags=c->rotation|((uint32_t)c->mirror<<2)|((uint32_t)c->flip<<3);
  s_started=now;s_offset=0;s_crc=0xFFFFFFFFU;s_stage=1;
 }
 if(s_stage==1){
  n=FRAME_BYTES-s_offset;if(n>2048U)n=2048U;
  s_crc=VideoCrc(s_crc,s_frame+s_offset,n);s_offset+=n;
  if(s_offset!=FRAME_BYTES)return;
  memset(s_header,0,sizeof(s_header));memcpy(s_header,"H743IMG1",8);
  s_header[8]=1;s_header[9]=1;s_header[10]=(uint8_t)s_flags;s_header[11]=VIDEO_HEADER_SIZE;
  VideoPut32(s_header+12,s_sequence);VideoPut32(s_header+16,s_tick);
  VideoPut16(s_header+20,BOARD_CAMERA_WIDTH);VideoPut16(s_header+22,BOARD_CAMERA_HEIGHT);
  VideoPut32(s_header+24,FRAME_BYTES);VideoPut32(s_header+28,s_crc^0xFFFFFFFFU);
  VideoPut32(s_header+36,VideoCrc(0xFFFFFFFFU,s_header,36)^0xFFFFFFFFU);s_stage=2;
 }
 if(s_stage==2 && UsbConsole_StreamStart(s_header,sizeof(s_header),s_frame,FRAME_BYTES))s_stage=3;
}
