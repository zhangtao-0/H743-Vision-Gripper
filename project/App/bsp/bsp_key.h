/*********************************************************************************************************************
 * @file    bsp_key.h
 * @brief   通用按键扫描与事件接口。
 *********************************************************************************************************************/

#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "input_control.h"
#include "input_config.h"
#include "board_config.h"
#include <stdint.h>

#define KEY_DEBOUNCE_TIME_MS             INPUT_KEY_DEBOUNCE_MS
#define KEY_LONG_PRESS_TIME_MS           INPUT_KEY_LONG_PRESS_MS
#define KEY_MULTI_CLICK_TIME_MS          INPUT_KEY_MULTI_CLICK_MS

typedef enum {
    KEY_TRIGGER_NONE = 0U,
    KEY_TRIGGER_CLICK,
    KEY_TRIGGER_LONG_PRESS,
} KeyTriggerMode_t;

typedef enum {
    KEY_EVENT_NONE         = 0x0000U,
    KEY_EVENT_PRESS        = 0x0001U,
    KEY_EVENT_RELEASE      = 0x0002U,
    KEY_EVENT_SINGLE_CLICK = 0x0004U,
    KEY_EVENT_DOUBLE_CLICK = 0x0008U,
    KEY_EVENT_TRIPLE_CLICK = 0x0010U,
    KEY_EVENT_LONG_START   = 0x0020U,
    KEY_EVENT_LONG_RELEASE = 0x0040U,
} KeyEvent_t;

typedef struct {
    uint8_t pressed;
    uint8_t long_reported;
    uint8_t click_count;
    uint32_t press_start_tick;
    uint32_t click_tick;
    uint16_t events;
} KeyGesture_t;

#define KEY_Task1_PORT                   BOARD_KEY1_PORT
#define KEY_Task1_PIN                    BOARD_KEY1_PIN
#define KEY_Task1_MODE                   GPIO_MODE_INPUT
#define KEY_Task1_PULL                   BOARD_KEY1_PULL
#define KEY_Task1_SPEED                  GPIO_SPEED_FREQ_LOW
#define KEY_Task1_ACTIVE_STATE           BOARD_KEY1_ACTIVE_STATE

#define KEY_Task2_PORT                   BOARD_KEY2_PORT
#define KEY_Task2_PIN                    BOARD_KEY2_PIN
#define KEY_Task2_MODE                   GPIO_MODE_INPUT
#define KEY_Task2_PULL                   BOARD_KEY2_PULL
#define KEY_Task2_SPEED                  GPIO_SPEED_FREQ_LOW
#define KEY_Task2_ACTIVE_STATE           BOARD_KEY2_ACTIVE_STATE

#define KEY_Task3_PORT                   BOARD_KEY3_PORT
#define KEY_Task3_PIN                    BOARD_KEY3_PIN
#define KEY_Task3_MODE                   GPIO_MODE_INPUT
#define KEY_Task3_PULL                   BOARD_KEY3_PULL
#define KEY_Task3_SPEED                  GPIO_SPEED_FREQ_LOW
#define KEY_Task3_ACTIVE_STATE           BOARD_KEY3_ACTIVE_STATE

#define KEY_Task4_PORT                   BOARD_KEY4_PORT
#define KEY_Task4_PIN                    BOARD_KEY4_PIN
#define KEY_Task4_MODE                   GPIO_MODE_INPUT
#define KEY_Task4_PULL                   BOARD_KEY4_PULL
#define KEY_Task4_SPEED                  GPIO_SPEED_FREQ_LOW
#define KEY_Task4_ACTIVE_STATE           BOARD_KEY4_ACTIVE_STATE

#define KEY_Task5_PORT BOARD_KEY5_PORT
#define KEY_Task5_PIN BOARD_KEY5_PIN
#define KEY_Task5_MODE GPIO_MODE_INPUT
#define KEY_Task5_PULL BOARD_KEY5_PULL
#define KEY_Task5_SPEED GPIO_SPEED_FREQ_LOW
#define KEY_Task5_ACTIVE_STATE BOARD_KEY5_ACTIVE_STATE
#define KEY_Task6_PORT BOARD_KEY6_PORT
#define KEY_Task6_PIN BOARD_KEY6_PIN
#define KEY_Task6_MODE GPIO_MODE_INPUT
#define KEY_Task6_PULL BOARD_KEY6_PULL
#define KEY_Task6_SPEED GPIO_SPEED_FREQ_LOW
#define KEY_Task6_ACTIVE_STATE BOARD_KEY6_ACTIVE_STATE

typedef enum {
    Key_None  = 0U,
    Key_Task1 = 1U,
    Key_Task2 = 2U,
    Key_Task3 = 3U,
    Key_Task4 = 4U,
    Key_Task5 = 5U,
    Key_Task6 = 6U,
    Key_Count = 6U,
    Key_TableSize
} KeyCode_t;

void Key_Init(void);
void Key_Task(void);
uint16_t Key_TakeEvent(KeyCode_t code);
uint8_t Key_Scan(KeyCode_t code);
uint8_t Key_ReadAndClear(KeyCode_t code);
uint8_t Key_GetFlag(KeyCode_t code);
void Key_Clear(KeyCode_t code);
void Key_ClearAll(void);
void Key_Toggle(KeyCode_t code);
void Key_SetFlag(KeyCode_t code, uint8_t value);
void KeyGesture_Init(KeyGesture_t *gesture);
void KeyGesture_SetPressed(KeyGesture_t *gesture, uint8_t pressed, uint32_t now);
uint16_t KeyGesture_Task(KeyGesture_t *gesture, uint32_t now);

#ifdef __cplusplus
}
#endif

#endif