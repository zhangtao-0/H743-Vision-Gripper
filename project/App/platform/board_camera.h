#ifndef BOARD_CAMERA_H
#define BOARD_CAMERA_H
#include "platform_hal.h"
typedef struct {
    uint32_t frames, errors, timeouts, frame_tick;
    uint32_t last_dcmi_error, last_dma_error, remaining_words;
    uint16_t format, pclk;
    uint16_t preview_row;
    uint8_t ready, enabled, capturing, displaying;
    uint8_t rotation,mirror,flip,edges,frozen,frame_valid;
} BoardCameraStatus;
HAL_StatusTypeDef BoardCamera_Init(void);
void BoardCamera_Task(uint32_t now);
void BoardCamera_Enable(uint8_t enable);
HAL_StatusTypeDef BoardCamera_Snapshot(void);
void BoardCamera_Resume(void);
void BoardCamera_SetTransform(uint8_t rotation,uint8_t mirror,uint8_t flip);
void BoardCamera_SetEdges(uint8_t enable);
void BoardCamera_SetPreview(uint8_t enable,uint16_t top,uint16_t rows);
HAL_StatusTypeDef BoardCamera_SetPclk(uint8_t divider);
const BoardCameraStatus *BoardCamera_GetStatus(void);
uint16_t *BoardCamera_Frame(void);
/* Main-loop lease: prevents a new DMA capture until released. */
const uint16_t *BoardCamera_AcquireFrame(uint32_t *sequence,uint32_t *tick);
void BoardCamera_ReleaseFrame(void);
void BoardCamera_Irq(void);
void BoardCamera_DmaIrq(void);
#endif
