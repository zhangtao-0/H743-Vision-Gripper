/* Integrated H743: vendor peripherals + servo stack share one cooperative loop. */
#include "app_main.h"
#include "input_control.h"
#include "board_config.h"
#include "board_uart.h"
#include "board_led.h"
#include "bsp_key.h"
#include "feature_config.h"
#include "vofa_protocol.h"
#include "servo_p3.h"
#include "integration_service.h"
#include "board_camera.h"
#include "integration_config.h"
#include "motor_service.h"
#include "ui_service.h"
#include "ui_settings.h"
#include "vendor_log.h"
#include "feetech_sts_bus.h"
#if APP_P3_MOTION_ENABLE
#include "feetech_sts_command.h"
#include "feetech_sts_control.h"
#endif

static uint32_t s_boot_tick;
static uint8_t s_resources_ok;
static uint8_t s_motion_started;
/* 调试器可观察每路去抖后的按下事件。 */
volatile uint32_t g_p3_key_press_count[6];

void App_Init(void)
{
#if APP_STARTUP_LOG_ENABLE
    static const uint8_t boot[] =
        "\r\n[H743] integrated-v1 LCD/CAM/RTC/SD/QSPI/USB + STS; HOST=115200\r\n";
#endif

    VendorLog_Enable(APP_STARTUP_LOG_ENABLE);
    BoardLed_Init();
    Key_Init();

    s_resources_ok = (BoardUart_InitAll() == BOARD_OK);

    if (s_resources_ok) {
        VOFA_Protocol_Init();
    }

#if APP_STARTUP_LOG_ENABLE
    (void)BoardUart_Transmit(BOARD_UART_HOST, boot,
                             (uint16_t)(sizeof(boot) - 1U), 50U);
#endif

    if (s_resources_ok) Integration_Init();

#if APP_P3_SERVO_ENABLE
    if (s_resources_ok) {
        ServoP3_Init();
    }
#endif

    s_motion_started = 0U;
    s_boot_tick = HAL_GetTick();
    VendorLog_Enable(1);
}

#if APP_P3_MOTION_ENABLE
/* 只读探测确认在线后，才把总线交给原有命令/控制层；上电本身不下发运动目标。 */
static void App_StartMotionStack(void)
{
    FeetechStsBus_Init();
    FeetechStsControl_Init();
    UiSettings_ApplySts();
    FeetechStsCommand_Init();
    s_motion_started = 1U;
}
#endif

void App_Task(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - s_boot_tick;
    uint32_t i;
    uint8_t pressed = 0U;

    Key_Task();
#if BOARD_ENABLE_UI
    UiService_Input(now, Key_GetFlag(Key_Task1), Key_GetFlag(Key_Task2), Key_GetFlag(Key_Task3), Key_GetFlag(Key_Task4));
#endif
    for (i = 0U; i < (uint32_t)Key_Count; i++) {
        KeyCode_t key = (KeyCode_t)(i + 1U);
        uint16_t events = Key_TakeEvent(key);
#if !BOARD_KEYS34_CONTROL_PWM && !BOARD_ENABLE_UI
        if (key == Key_Task3 && (events & KEY_EVENT_SINGLE_CLICK)) BoardCamera_Enable(!BoardCamera_GetStatus()->enabled);
        if (key == Key_Task4 && (events & KEY_EVENT_SINGLE_CLICK)) Integration_StatusReport();
#endif
        if(Input_DebugEnabled(1)){
            if(key==Key_Task5 && (events & KEY_EVENT_SINGLE_CLICK))
                (void)MotorService_Enable(!MotorService_IsRunning());
            if(key==Key_Task6 && (events & (KEY_EVENT_SINGLE_CLICK|KEY_EVENT_DOUBLE_CLICK))){
                MotorStatus motor;int32_t rpm;
                MotorService_Snapshot(&motor);
                rpm=(int32_t)motor.control.target_rpm+((events&KEY_EVENT_DOUBLE_CLICK)?5:-5);
                if(rpm<0)rpm=0;if(rpm>(int32_t)motor.control.config.max_rpm)rpm=(int32_t)motor.control.config.max_rpm;
                (void)MotorService_SetTarget((uint16_t)rpm);
            }
        }
        if ((events & KEY_EVENT_PRESS) != 0U) {
            g_p3_key_press_count[i]++;
        }
        if (Key_GetFlag(key) != 0U) {
            pressed = 1U;
        }
    }

    if (s_resources_ok) {
        VOFA_Protocol_Task();
#if APP_P3_SERVO_ENABLE
        if (s_motion_started == 0U) {
            ServoP3_Task(now);
#if APP_P3_MOTION_ENABLE
            if (ServoP3_GetStatus()->state == (uint8_t)SERVO_P3_STATE_ONLINE &&
                ServoP3_GetStatus()->reported_id == BOARD_SERVO_ID &&
                ServoP3_GetStatus()->last_error == 0U) {
                App_StartMotionStack();
            }
#endif
        }
#endif
#if APP_P3_MOTION_ENABLE
        if (s_motion_started != 0U) {
            FeetechStsCommand_Task();
        }
#endif
    }

    /* KEY5 只读阶段重新探测；接入 command/control 后按键交给原状态机。 */
    if (Input_DebugEnabled(0) && (s_motion_started == 0U) && (g_p3_key_press_count[4] != 0U)) {
        static uint32_t last_probe_key_count;
        if (last_probe_key_count != g_p3_key_press_count[4]) {
            last_probe_key_count = g_p3_key_press_count[4];
#if APP_P3_SERVO_ENABLE
            ServoP3_RequestProbe();
#endif
        }
    }

    if (!s_resources_ok) {
        BoardLed_Set(BOARD_LED_HEARTBEAT, BOARD_LED_BLINK_5HZ);
        BoardLed_Set(BOARD_LED_STATUS, BOARD_LED_OFF);
        BoardLed_Set(BOARD_LED_FAULT, BOARD_LED_ON);
    } else if (elapsed < 800U) {
        BoardLed_Set(BOARD_LED_HEARTBEAT,
            (elapsed >= 200U && ((elapsed - 200U) % 200U) < 100U) ? BOARD_LED_ON : BOARD_LED_OFF);
        BoardLed_Set(BOARD_LED_STATUS, BOARD_LED_OFF);
        BoardLed_Set(BOARD_LED_FAULT, BOARD_LED_OFF);
    } else {
#if APP_P3_MOTION_ENABLE
        if (s_motion_started != 0U) {
            const FeetechStsFeedback_t *feedback = FeetechStsBus_GetFeedback();
            BoardLed_Set(BOARD_LED_HEARTBEAT, BOARD_LED_HEARTBEAT_1HZ);
            BoardLed_Set(BOARD_LED_STATUS,
                         (feedback->online != 0U) ?
                         (feedback->moving != 0U ? BOARD_LED_BLINK_5HZ : BOARD_LED_ON) :
                         BOARD_LED_BLINK_5HZ);
            BoardLed_Set(BOARD_LED_FAULT,
                         ((feedback->online == 0U) || (feedback->error != 0U)) ?
                         BOARD_LED_BLINK_2HZ : BOARD_LED_OFF);
        } else
#endif
        {
#if APP_P3_SERVO_ENABLE
            const ServoP3Status *servo = ServoP3_GetStatus();
            BoardLed_Set(BOARD_LED_HEARTBEAT, BOARD_LED_HEARTBEAT_1HZ);
            BoardLed_Set(BOARD_LED_STATUS,
                         (servo->online != 0U) ? BOARD_LED_ON : BOARD_LED_BLINK_5HZ);
            BoardLed_Set(BOARD_LED_FAULT,
                         ((servo->online == 0U) || (servo->last_error != 0U)) ?
                         BOARD_LED_BLINK_2HZ : BOARD_LED_OFF);
#else
            BoardLed_Set(BOARD_LED_HEARTBEAT, BOARD_LED_HEARTBEAT_1HZ);
            BoardLed_Set(BOARD_LED_STATUS, pressed ? BOARD_LED_BLINK_5HZ : BOARD_LED_ON);
            BoardLed_Set(BOARD_LED_FAULT, BOARD_LED_OFF);
#endif
        }
    }

    Integration_Task(HAL_GetTick());
    if (!Integration_GetStatus()->clocks_ok) BoardLed_Set(BOARD_LED_FAULT, BOARD_LED_ON);
#if BOARD_ENABLE_CAMERA
    else if (!BoardCamera_GetStatus()->ready || BoardCamera_GetStatus()->errors || BoardCamera_GetStatus()->timeouts)
        BoardLed_Set(BOARD_LED_FAULT, BOARD_LED_BLINK_2HZ);
#endif
    {
        MotorStatus motor;MotorService_Snapshot(&motor);
        if(motor.control.fault) BoardLed_Set(BOARD_LED_FAULT,BOARD_LED_BLINK_5HZ);
    }
    BoardLed_Task(HAL_GetTick());
    (void)pressed;
}

unsigned char App_MotionReady(void) { return s_motion_started; }
