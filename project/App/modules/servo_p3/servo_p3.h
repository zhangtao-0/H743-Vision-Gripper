/*********************************************************************************************************************
 * @file    servo_p3.h
 * @brief   P3 舵机只读识别与反馈服务。
 *
 * @details
 * 1. 只使用 PING/READ/READ_FEEDBACK，不写扭矩、目标、模式或 EEPROM。
 * 2. 先探测 ID/型号/波特率索引/角度限位，再周期读取位置/电压/温度/电流。
 * 3. 结果通过 VOFA(HOST, USART1) 文本行输出，供第一轮板上只读验证。
 *********************************************************************************************************************/

#ifndef SERVO_P3_H
#define SERVO_P3_H

#include "platform_hal.h"
#include <stdint.h>

typedef enum {
    SERVO_P3_STATE_IDLE = 0,
    SERVO_P3_STATE_PING,
    SERVO_P3_STATE_READ_ID,
    SERVO_P3_STATE_READ_MODEL,
    SERVO_P3_STATE_READ_BAUD,
    SERVO_P3_STATE_READ_LIMITS,
    SERVO_P3_STATE_ONLINE,
    SERVO_P3_STATE_RETRY
} ServoP3State;

typedef struct {
    uint8_t  state;              /* ServoP3State */
    uint8_t  online;
    uint8_t  reported_id;
    uint8_t  baud_index;
    uint8_t  baud_locked;
    uint8_t  baud_step;
    uint8_t  scan_active;
    uint32_t mcu_baud;
    uint8_t  last_error;
    uint16_t model;
    uint16_t min_position;
    uint16_t max_position;

    uint16_t position;
    int32_t  angle_tenths;       /* 角度 ×10，避免 printf 浮点 */
    uint16_t voltage_mv;
    uint8_t  temperature_c;
    uint16_t current_ma;
    int16_t  load;
    uint8_t  moving;

    uint32_t ping_attempts;
    uint32_t responses;
    uint32_t timeouts;
    uint32_t parse_errors;
    uint32_t tx_errors;
    uint32_t last_rx_tick;
} ServoP3Status;

void ServoP3_SetTelemetry(uint8_t enable);
void ServoP3_Init(void);
void ServoP3_Task(uint32_t now_ms);
void ServoP3_RequestProbe(void);
uint8_t ServoP3_ProbeAt(uint8_t id, uint32_t baud);
uint8_t ServoP3_StartScan(uint32_t baud);
const ServoP3Status *ServoP3_GetStatus(void);

#endif /* SERVO_P3_H */
