#include "board_camera.h"
#include "integration_config.h"
#include "lcd_spi_154.h"
#if BOARD_CAMERA_SENSOR == 5640
#include "dcmi_ov5640.h"
#define Camera_Init DCMI_OV5640_Init
#define Camera_FrameFlag OV5640_FrameState
#define Camera_Stop OV5640_DCMI_Stop
#else
#include "dcmi_ov2640.h"
#define Camera_Init DCMI_OV2640_Init
#define Camera_FrameFlag DCMI_FrameState
#define Camera_Stop OV2640_DCMI_Stop
#endif
extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef DMA_Handle_dcmi;

/* IAR section is explicitly placed in noncacheable DMA-visible AXI SRAM.
 * Keep normal data/stack in DTCM; never send a DTCM pointer to DMA2. */
#pragma data_alignment=32
#pragma location=".camera_dma"
__no_init static uint16_t s_frame[BOARD_CAMERA_WIDTH * BOARD_CAMERA_HEIGHT];
static BoardCameraStatus s_status;
static uint8_t s_snapshot_pending, s_frame_leased;
static uint16_t s_strip[BOARD_CAMERA_WIDTH*BOARD_CAMERA_STRIP_ROWS];
static uint32_t s_started, s_next;
static uint8_t s_preview_enabled = !BOARD_ENABLE_UI;
static uint16_t s_preview_top, s_preview_rows = BOARD_CAMERA_HEIGHT;

HAL_StatusTypeDef BoardCamera_Init(void)
{
#if !BOARD_ENABLE_CAMERA
    return HAL_ERROR;
#else
    uint32_t errors = SCCB_ErrorCount;
    s_status.frame_valid=s_status.frozen=s_snapshot_pending=0;
    s_status.capturing=s_status.displaying=0;
    s_status.ready = (Camera_Init() == 0 && SCCB_ErrorCount == errors);
    s_status.enabled = s_status.ready;
#if BOARD_CAMERA_SENSOR == 5640
    if (s_status.ready) {
        s_status.format = ((uint16_t)SCCB_ReadReg_16Bit(0x4300) << 8) | SCCB_ReadReg_16Bit(0x501F);
        s_status.pclk = ((uint16_t)SCCB_ReadReg_16Bit(0x460C) << 8) | SCCB_ReadReg_16Bit(0x3824);
        if (SCCB_ErrorCount != errors || s_status.format != 0x6F01U || (s_status.pclk & 0x02FFU) != (0x0200U | BOARD_CAMERA_PCLK_DIV))
            s_status.ready = s_status.enabled = 0;
    }
#endif
    if (!s_status.ready) { s_status.errors++; return HAL_ERROR; }
    return HAL_OK;
#endif
}

void BoardCamera_Enable(uint8_t enable)
{
    s_status.enabled = (enable && s_status.ready);
    if (!s_status.enabled && s_status.capturing) {
        Camera_Stop(); s_status.capturing = 0;
    }
    if (!s_status.enabled) s_status.displaying = 0;
}

void BoardCamera_SetPreview(uint8_t enable,uint16_t top,uint16_t rows)
{
    if(!rows || top>=BOARD_CAMERA_HEIGHT || rows>BOARD_CAMERA_HEIGHT-top)enable=0;
    enable=(uint8_t)(enable!=0);
    if(enable!=s_preview_enabled || top!=s_preview_top || rows!=s_preview_rows){
        s_status.displaying=(uint8_t)(enable && s_status.frozen && s_status.frame_valid); s_status.preview_row=0;
        s_preview_enabled=enable;s_preview_top=top;s_preview_rows=rows;
    }
}

static void Redraw(void){
 s_status.preview_row=0;s_status.displaying=(uint8_t)(s_preview_enabled&&s_status.frozen&&s_status.frame_valid);
}
HAL_StatusTypeDef BoardCamera_Snapshot(void){
 if(!s_status.ready)return HAL_ERROR;
 s_status.frozen=0;s_snapshot_pending=1;s_status.enabled=1;return HAL_OK;
}
void BoardCamera_Resume(void){s_snapshot_pending=0;s_status.frozen=0;BoardCamera_Enable(1);}
void BoardCamera_SetTransform(uint8_t r,uint8_t mirror,uint8_t flip){s_status.rotation=r&3U;s_status.mirror=(mirror!=0);s_status.flip=(flip!=0);Redraw();}
void BoardCamera_SetEdges(uint8_t enable){s_status.edges=(enable!=0);Redraw();}
/* Destination transform: rotate clockwise, then mirror horizontally / flip vertically.
 * Raw RGB565 capture is never modified, so future vision uses known sensor coordinates. */
static uint16_t Pixel(uint16_t x,uint16_t y){
 uint16_t sx=x,sy=y;
 if(s_status.mirror)x=(uint16_t)(BOARD_CAMERA_WIDTH-1U-x);
 if(s_status.flip)y=(uint16_t)(BOARD_CAMERA_HEIGHT-1U-y);
 switch(s_status.rotation){
 case 1:sx=y;sy=(uint16_t)(BOARD_CAMERA_HEIGHT-1U-x);break;
 case 2:sx=(uint16_t)(BOARD_CAMERA_WIDTH-1U-x);sy=(uint16_t)(BOARD_CAMERA_HEIGHT-1U-y);break;
 case 3:sx=(uint16_t)(BOARD_CAMERA_WIDTH-1U-y);sy=x;break;
 default:sx=x;sy=y;break;
 }
 return s_frame[(uint32_t)sy*BOARD_CAMERA_WIDTH+sx];
}
static int Luma(uint16_t p){return (int)(((p>>11)&31U)*2U+((p>>5)&63U)*3U+(p&31U)*2U);}
static uint16_t DisplayPixel(uint16_t x,uint16_t y){
 int dx,dy;
 if(!s_status.edges)return Pixel(x,y);
 if(x+1U>=BOARD_CAMERA_WIDTH||y+1U>=BOARD_CAMERA_HEIGHT)return 0;
 dx=Luma(Pixel(x+1U,y))-Luma(Pixel(x,y));dy=Luma(Pixel(x,y+1U))-Luma(Pixel(x,y));
 if(dx<0)dx=-dx;if(dy<0)dy=-dy;
 return dx+dy>=48?0xFFFFU:0;
}
HAL_StatusTypeDef BoardCamera_SetPclk(uint8_t divider)
{
#if BOARD_CAMERA_SENSOR == 5640
    uint32_t errors = SCCB_ErrorCount;
    uint8_t control;
    if (!s_status.ready || (divider != 0 && divider != 1 && divider != 2 && divider != 4 && divider != 8 && divider != 16)) return HAL_ERROR;
    BoardCamera_Enable(0);
    control = SCCB_ReadReg_16Bit(0x460C);
    SCCB_WriteReg_16Bit(0x460C, divider ? control | 2U : control & ~2U);
    if (divider) SCCB_WriteReg_16Bit(0x3824, divider);
    HAL_Delay(100);
    s_status.pclk = ((uint16_t)SCCB_ReadReg_16Bit(0x460C) << 8) | SCCB_ReadReg_16Bit(0x3824);
    if (SCCB_ErrorCount != errors ||
        (divider ? ((s_status.pclk & 0x02FFU) != (0x0200U | divider)) : ((s_status.pclk & 0x0200U) != 0U)))
        {s_status.errors++;return HAL_ERROR;}
    BoardCamera_Enable(1);return HAL_OK;
#else
    (void)divider; return HAL_ERROR;
#endif
}

void BoardCamera_Task(uint32_t now)
{
    if (!s_status.ready || !s_status.enabled) return;
    if (s_status.capturing) {
        /* Frame IRQ alone is not sufficient: DMA must have completed as well. */
        if (Camera_FrameFlag && DMA_Handle_dcmi.State == HAL_DMA_STATE_READY &&
            hdcmi.ErrorCode == HAL_DCMI_ERROR_NONE && DMA_Handle_dcmi.ErrorCode == HAL_DMA_ERROR_NONE &&
            __HAL_DMA_GET_COUNTER(&DMA_Handle_dcmi) == 0U) {
            Camera_FrameFlag = 0;
            (void)HAL_DCMI_Stop(&hdcmi);
            __DMB();
            s_status.capturing = 0; s_status.displaying = s_preview_enabled; s_status.preview_row = 0;
            if (!s_status.displaying) s_next = now + 50U;
            s_status.frames++;s_status.frame_tick=now;s_status.frame_valid=1;
            if(s_snapshot_pending){s_snapshot_pending=0;s_status.frozen=1;}
        } else if (hdcmi.ErrorCode != HAL_DCMI_ERROR_NONE || (uint32_t)(now - s_started) > BOARD_CAMERA_FRAME_TIMEOUT_MS) {
            s_status.last_dcmi_error = hdcmi.ErrorCode;
            s_status.last_dma_error = DMA_Handle_dcmi.ErrorCode;
            s_status.remaining_words = __HAL_DMA_GET_COUNTER(&DMA_Handle_dcmi);
            Camera_Stop(); s_status.capturing = 0; s_status.timeouts++;
            s_next = now + 100U;
        }
    } else if (s_status.displaying) {
#if BOARD_ENABLE_LCD
        uint16_t row = s_status.preview_row;
        uint16_t rows = BOARD_CAMERA_STRIP_ROWS;
        if (row + rows > s_preview_rows) rows = (uint16_t)(s_preview_rows - row);
        /* Fit the complete square image, letterbox horizontally in the menu viewport. */
        uint16_t yy,xx,side=s_preview_rows<BOARD_CAMERA_WIDTH?s_preview_rows:BOARD_CAMERA_WIDTH;
        uint16_t left=(uint16_t)((BOARD_CAMERA_WIDTH-side)/2U);
        for(yy=0;yy<rows;yy++)for(xx=0;xx<BOARD_CAMERA_WIDTH;xx++){
            uint16_t color=0;
            if(xx>=left && xx<left+side){
                uint16_t sx=(uint16_t)(((uint32_t)(xx-left)*BOARD_CAMERA_WIDTH)/side);
                uint16_t sy=(uint16_t)(((uint32_t)(row+yy)*BOARD_CAMERA_HEIGHT)/side);
                color=DisplayPixel(sx,sy);
            }
            s_strip[yy*BOARD_CAMERA_WIDTH+xx]=color;
        }
        LCD_CopyBuffer(0,(uint16_t)(s_preview_top+row),BOARD_CAMERA_WIDTH,rows,s_strip);
        s_status.preview_row = (uint16_t)(row + rows);
        if (s_status.preview_row >= s_preview_rows) { s_status.displaying = 0; s_next = now + 50U; }
#else
        s_status.displaying = 0; s_next = now + 100U;
#endif
    } else if (!s_frame_leased && !s_status.frozen && (int32_t)(now - s_next) >= 0) {
        Camera_FrameFlag = 0;
        hdcmi.ErrorCode = HAL_DCMI_ERROR_NONE;
        DMA_Handle_dcmi.Init.Mode = DMA_NORMAL;
        if (HAL_DMA_Init(&DMA_Handle_dcmi) == HAL_OK &&
            HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)s_frame,
                             sizeof(s_frame) / 4U) == HAL_OK) {
            s_status.capturing = 1; s_started = now;
        } else { s_status.errors++; s_next = now + 100U; }
    }
}

const BoardCameraStatus *BoardCamera_GetStatus(void) { return &s_status; }
uint16_t *BoardCamera_Frame(void) { return s_frame; }
const uint16_t *BoardCamera_AcquireFrame(uint32_t *sequence,uint32_t *tick) {
 if(!s_status.ready || !s_status.frame_valid || s_status.capturing || s_frame_leased)return 0;
 s_frame_leased=1;*sequence=s_status.frames;*tick=s_status.frame_tick;return s_frame;
}
void BoardCamera_ReleaseFrame(void) { s_frame_leased=0; }
void BoardCamera_Irq(void) { HAL_DCMI_IRQHandler(&hdcmi); }
void BoardCamera_DmaIrq(void) { HAL_DMA_IRQHandler(&DMA_Handle_dcmi); }
