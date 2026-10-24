#include "integration_service.h"
#include "integration_config.h"
#include "feature_config.h"
#include "integration_console.h"
#include "board_peripherals.h"
#include "board_storage.h"
#include "board_camera.h"
#include "usb_console.h"
#include "vision_link.h"
#include "lcd_spi_154.h"
#include "bsp_bluetooth.h"
#include "feetech_sts_bus.h"
#include "pwm_servo_control.h"
#include "vofa_protocol.h"
#include "vendor_log.h"
#include "servo_p3.h"
#include "board_uart.h"
#include "motor_service.h"
#include "motor_plot.h"
#include "ui_service.h"
#include "ui_settings.h"
#include <stdio.h>
extern SPI_HandleTypeDef hSPI6;
static IntegrationStatus s_status;
static uint32_t s_last_task, s_next_report, s_next_stream;
static uint8_t s_csv_stream;
void Integration_SetStream(uint8_t enable) { s_csv_stream = (enable != 0U); }

void Integration_Init(void)
{
    uint32_t start = HAL_GetTick();
    s_status.clocks_ok = (BoardPeripherals_ClockInit() == HAL_OK);
    if (!s_status.clocks_ok) { VendorLog("peripheral clocks failed\r\n"); return; }
#if BOARD_ENABLE_LCD
    SPI_LCD_Init(); s_status.lcd_bus_ready = hSPI6.State == HAL_SPI_STATE_READY;
    if (s_status.lcd_bus_ready) {
        LCD_SetAsciiFont(&ASCII_Font16); LCD_SetBackColor(LCD_BLACK); LCD_SetColor(LCD_WHITE);
        LCD_DisplayString(0, 0, "H743 Vision Gripper");
    }
#endif
#if BOARD_ENABLE_RTC
    s_status.rtc_ready = (BoardRtc_Init() == HAL_OK);
#endif
    BoardStorage_Init();
#if BOARD_ENABLE_USB_CDC
    UsbConsole_Init();
#endif
#if BOARD_ENABLE_CAMERA
    s_status.camera_ready = (BoardCamera_Init() == HAL_OK);
#endif
#if BOARD_ENABLE_BLUETOOTH
    Bluetooth_Init();
#endif
#if BOARD_ENABLE_PWM
    PwmServoControl_Init();
#endif
    MotorService_Init();
    MotorPlot_Init();
    UiSettings_Init();
#if BOARD_ENABLE_UI
    UiService_Init();
#endif
    s_status.last_init_ms = HAL_GetTick() - start;
    s_last_task = HAL_GetTick(); s_next_report = s_last_task + 1000;
#if APP_STARTUP_LOG_ENABLE
    Integration_StatusReport();
#endif
}

void Integration_LcdTest(void)
{
    if (!s_status.lcd_bus_ready) return;
    BoardCamera_Enable(0);
#if BOARD_ENABLE_UI
    UiService_LcdTest(1);
#endif
    LCD_SetBackColor(LCD_BLACK); LCD_Clear();
    LCD_SetColor(LCD_RED); LCD_FillRect(0,0,80,64);
    LCD_SetColor(LCD_GREEN); LCD_FillRect(80,0,80,64);
    LCD_SetColor(LCD_BLUE); LCD_FillRect(160,0,80,64);
    LCD_SetColor(LCD_WHITE); LCD_DisplayString(0,80,"H743 LCD SPI6 test");
}

void Integration_StatusReport(void)
{
    const ServoP3Status *probe = ServoP3_GetStatus();
    VendorLog("STS target=%u rate=%lu rawRx=%lu valid=%lu parser=%lu tx=%lu hal=%d\r\n",
        probe->reported_id, (unsigned long)BoardUart_GetBaud(BOARD_UART_SERVO),
        (unsigned long)FeetechStsBus_RawRxBytes(), (unsigned long)probe->responses,
        (unsigned long)FeetechStsBus_GetFeedback()->parse_error_count,
        (unsigned long)FeetechStsBus_GetBus()->tx.tx_count, (int)FeetechStsBus_GetLastStatus());
    const BoardCameraStatus *c = BoardCamera_GetStatus();
    const BoardStorageStatus *s = BoardStorage_GetStatus();
    VendorLog("SYS clk=%u LCDbus=%u RTC=%u SD=%u FS=%u QSPI=%u id=%06lX USB=%u\r\n",
        s_status.clocks_ok,s_status.lcd_bus_ready,s_status.rtc_ready,s->sd_ready,s->fs_ready,
        s->qspi_ready,(unsigned long)s->qspi_id,UsbConsole_Connected());
    VendorLog("CAM diag format=%04X pclk=%04X dcmi=%08lX dma=%08lX remaining=%lu\r\n",
        c->format,c->pclk,(unsigned long)c->last_dcmi_error,(unsigned long)c->last_dma_error,(unsigned long)c->remaining_words);
    VendorLog("CAM ready=%u enabled=%u frames=%lu errors=%lu timeout=%lu loopmax=%lu cmdDrop=%lu usbDrop=%lu\r\n",
        c->ready,c->enabled,(unsigned long)c->frames,(unsigned long)c->errors,(unsigned long)c->timeouts,
        (unsigned long)s_status.loop_max_ms,(unsigned long)IntegrationConsole_Dropped(),(unsigned long)UsbConsole_Dropped());
}

void Integration_Task(uint32_t now)
{
    uint32_t delta = now - s_last_task;
    if (delta > s_status.loop_max_ms) s_status.loop_max_ms = delta;
    s_last_task = now;
    IntegrationConsole_Task();
#if BOARD_ENABLE_PWM
    PwmServoControl_Task();
#endif
#if BOARD_ENABLE_USB_CDC
    UsbConsole_Task();
#endif
#if BOARD_ENABLE_BLUETOOTH
    Bluetooth_Task();
#endif
#if BOARD_ENABLE_UI
    UiService_Task(now);
#endif
#if BOARD_ENABLE_CAMERA
    if (s_status.clocks_ok) BoardCamera_Task(now);
#endif
    VisionLink_Task(now);
    MotorPlot_Task(now);
    if (s_csv_stream && (int32_t)(now - s_next_stream) >= 0) {
        const FeetechStsFeedback_t *f = FeetechStsBus_GetFeedback();
        char csv[128]; int n;
        s_next_stream = now + 100U;
        n = snprintf(csv, sizeof(csv), "%lu,%u,%ld,%u,%u,%d,%u,%u,%lu\r\n",
            (unsigned long)now, f->position, (long)(f->angle_deg * 10.0f), f->voltage_mv,
            f->current_ma, f->load, f->online, f->moving, (unsigned long)BoardCamera_GetStatus()->frames);
        if (n > 0 && n < (int)sizeof(csv)) {
            (void)VOFA_Protocol_SendRaw((const uint8_t *)csv, (uint16_t)n);
            UsbConsole_Write((const uint8_t *)csv, (uint16_t)n);
        }
    }
    if ((int32_t)(now - s_next_report) >= 0) {
        s_next_report = now + 1000U;
        /* Update one text line when a snapshot is no longer being displayed. */
#if BOARD_ENABLE_LCD && !BOARD_ENABLE_UI
        if (s_status.lcd_bus_ready && !BoardCamera_GetStatus()->displaying) {
            const FeetechStsFeedback_t *fb = FeetechStsBus_GetFeedback();
            char line[32];
            snprintf(line, sizeof(line), "STS:%u POS:%4u F:%lu   ", fb->online, fb->position,
                     (unsigned long)BoardCamera_GetStatus()->frames);
            LCD_DisplayString(0, 224, line);
            {
                MotorStatus motor;MotorService_Snapshot(&motor);
                snprintf(line,sizeof(line),"M:%3ld R:%4ld D:%c E:%u  ",
                    (long)motor.control.target_rpm,(long)motor.control.measured_rpm,
                    motor.control.requested_direction>0?'+':'-',(unsigned)motor.control.fault);
                LCD_DisplayString(0,208,line);
            }
        }
#endif
    }
}
const IntegrationStatus *Integration_GetStatus(void) { return &s_status; }
