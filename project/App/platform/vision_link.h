#ifndef VISION_LINK_H
#define VISION_LINK_H
#include <stdint.h>
typedef struct {
 uint32_t sequence,frame_tick,received_tick;
 int32_t u,v,angle10,width,score;
 uint8_t valid;
} VisionAdvice;
void VisionLink_Task(uint32_t now);
uint8_t VisionLink_Enable(uint8_t enable);
uint8_t VisionLink_SetFps(uint32_t fps);
void VisionLink_Report(void);
void VisionLink_Clear(void);
uint8_t VisionLink_Accept(uint32_t seq,int32_t u,int32_t v,int32_t angle10,int32_t width,int32_t score);
const VisionAdvice *VisionLink_Advice(uint32_t now);
#endif
