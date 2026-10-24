/*********************************************************************************************************************
 * @file    servo_p3.c
 * @brief   P3 舵机只读识别与反馈实现。
 *
 * @details
 * 1. 只发 PING / READ / READ_FEEDBACK；不写扭矩、目标、模式、ID 或 EEPROM。
 * 2. 非阻塞状态机：每次只挂一个请求，超时/失败转 RETRY，不长时间占用主循环。
 * 3. 遥测走 VOFA(HOST)；P3 默认不初始化 command/control，因此按键不会驱动舵机。
 *********************************************************************************************************************/

#include "servo_p3.h"
#include "board_config.h"
#include "board_uart.h"
#include "feature_config.h"
#include "feetech_sts_bus.h"
#include "vofa_protocol.h"

#include <stdio.h>
#include <string.h>

#define SERVO_P3_PROBE_TIMEOUT_MS       100U
#define SERVO_P3_PING_PERIOD_MS         300U
#define SERVO_P3_PING_ATTEMPTS          3U
#define SERVO_P3_RETRY_PERIOD_MS        2000U
#define SERVO_P3_FEEDBACK_PERIOD_MS     100U
#define SERVO_P3_OFFLINE_MS             800U

/* 先按源码的 1 Mbps 探测；不行再回退到常见 STS 速率。只改 MCU 侧，不写舵机。 */
static const uint32_t s_baud_candidates[] = {
    1000000U,   /* 原 F103 工程 USART2 配置 */
    9600U,      /* 仅作为常见候选，不代表从上游确认的默认速率 */
    500000U,
    250000U,
    128000U,
    115200U,
    76800U,
    57600U,
    38400U,
    19200U
};
#define SERVO_P3_BAUD_COUNT (sizeof(s_baud_candidates) / sizeof(s_baud_candidates[0]))

static ServoP3Status s_status;
static uint8_t  s_servo_id;
static uint8_t  s_waiting;
static uint32_t s_wait_start_tick;
static uint32_t s_next_action_tick;
static uint32_t s_next_telemetry_tick;
static uint8_t s_telemetry = APP_P3_TELEMETRY_ENABLE;
void ServoP3_SetTelemetry(uint8_t enable){s_telemetry=(uint8_t)(enable!=0);}

static const char *ServoP3_StateText(uint8_t state)
{
    switch (state) {
    case SERVO_P3_STATE_IDLE:         return "IDLE";
    case SERVO_P3_STATE_PING:         return "PING";
    case SERVO_P3_STATE_READ_ID:      return "READ_ID";
    case SERVO_P3_STATE_READ_MODEL:   return "READ_MODEL";
    case SERVO_P3_STATE_READ_BAUD:    return "READ_BAUD";
    case SERVO_P3_STATE_READ_LIMITS:  return "READ_LIMITS";
    case SERVO_P3_STATE_ONLINE:       return "ONLINE";
    case SERVO_P3_STATE_RETRY:        return "RETRY";
    default:                          return "?";
    }
}

/* 只改 MCU 侧 UART 波特率；不改舵机 EEPROM。 */
static void ServoP3_SetBaud(uint32_t baud)
{
    if (BoardUart_SetBaud(BOARD_UART_SERVO, baud) != BOARD_OK) {
        s_status.tx_errors++;
        return;
    }
    s_status.mcu_baud = baud;
    (void)FeetechStsBus_StartReceive();
}

/* 丢弃尚未取走的旧应答，避免重探测时把上一次反馈当成 PING/READ 应答。 */
static void ServoP3_DrainPacket(void)
{
    uint8_t buffer[FEETECH_STS_FRAME_MAX_LEN];
    uint8_t len;

    while (FeetechStsBus_TakePacket(buffer, (uint8_t)sizeof(buffer), &len) != 0U) {
        /* drain */
    }
}

static void ServoP3_ResetProbe(uint32_t now)
{
    ServoP3_DrainPacket();
    (void)FeetechStsBus_TakeUpdateFlags();
    if ((s_status.baud_locked == 0U) &&
        (s_status.mcu_baud != (uint32_t)BOARD_UART_SERVO_BAUD)) {
        ServoP3_SetBaud((uint32_t)BOARD_UART_SERVO_BAUD);
        s_status.baud_step = 0U;
    }
    s_status.online = 0U;
    s_status.state = (uint8_t)SERVO_P3_STATE_PING;
    s_status.ping_attempts = 0U;
    s_waiting = 0U;
    s_next_action_tick = now;
    s_next_telemetry_tick = now;
}

static void ServoP3_EnterRetry(uint32_t now)
{
    s_waiting = 0U;
    s_status.online = 0U;
    s_status.state = (uint8_t)SERVO_P3_STATE_RETRY;
    s_next_action_tick = now + SERVO_P3_RETRY_PERIOD_MS;
}

static uint8_t ServoP3_StartRead(uint8_t address, uint8_t len, uint32_t now)
{
    if (FeetechStsBus_Read(s_servo_id, address, len) != HAL_OK) {
        s_status.tx_errors++;
        ServoP3_EnterRetry(now);
        return 0U;
    }

    s_waiting = 1U;
    s_wait_start_tick = now;
    return 1U;
}

/* 取一个有效包；只接受当前 ID。返回 1 表示拿到本舵机应答。 */
static uint8_t ServoP3_PollPacket(uint8_t *raw, uint8_t *raw_len, uint32_t now)
{
    if (FeetechStsBus_TakePacket(raw, FEETECH_STS_FRAME_MAX_LEN, raw_len) == 0U) {
        return 0U;
    }

    if (FeetechSts_IsValidFrame(raw, *raw_len) == 0U) {
        s_status.parse_errors++;
        return 0U;
    }

    if (raw[2] != s_servo_id) {
        return 0U;
    }

    {
        uint8_t expected = 6U;
        switch (s_status.state) {
            case SERVO_P3_STATE_READ_ID: case SERVO_P3_STATE_READ_BAUD: expected = 7U; break;
            case SERVO_P3_STATE_READ_MODEL: expected = 8U; break;
            case SERVO_P3_STATE_READ_LIMITS: expected = 10U; break;
            default: break;
        }
        /* Preserve device ERROR for diagnostics; it is not a format failure. */
        if (*raw_len != expected) { s_status.parse_errors++; return 0U; }
    }
    s_status.last_error = raw[4];
    s_status.last_rx_tick = now;
    s_status.responses++;
    return 1U;
}

static uint8_t ServoP3_ReadU8(uint8_t address, uint32_t now)
{
    return ServoP3_StartRead(address, 1U, now);
}

static void ServoP3_TaskFeedback(uint32_t now, uint8_t *raw, uint8_t *raw_len)
{
    uint8_t flags;
    const FeetechStsFeedback_t *feedback;

    if (s_waiting == 0U) {
        if ((int32_t)(now - s_next_action_tick) >= 0) {
            if (FeetechStsBus_ReadFeedback(s_servo_id) == HAL_OK) {
                s_waiting = 1U;
                s_wait_start_tick = now;
                s_next_action_tick = now + SERVO_P3_FEEDBACK_PERIOD_MS;
            } else {
                s_status.tx_errors++;
                s_next_action_tick = now + SERVO_P3_FEEDBACK_PERIOD_MS;
            }
        }
    }

    /* 解析器收到 READ 应答后会置更新位；这里在主循环上下文取走。 */
    flags = FeetechStsBus_TakeUpdateFlags();
    if (flags != 0U) {
        feedback = FeetechStsBus_GetFeedback();
        if (feedback != 0) {
            s_status.position = feedback->position;
            s_status.angle_tenths = (int32_t)(feedback->angle_deg * 10.0f);
            s_status.voltage_mv = feedback->voltage_mv;
            s_status.temperature_c = feedback->temperature_c;
            s_status.current_ma = feedback->current_ma;
            s_status.load = feedback->load;
            s_status.moving = feedback->moving;
            s_status.last_error = feedback->error;
            s_status.last_rx_tick = now;
            s_status.online = 1U;
        }
        s_waiting = 0U;
        ServoP3_DrainPacket();
    }

    if ((s_waiting != 0U) &&
        ((uint32_t)(now - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS)) {
        s_waiting = 0U;
        s_status.timeouts++;
        if ((uint32_t)(now - s_status.last_rx_tick) > SERVO_P3_OFFLINE_MS) {
            ServoP3_EnterRetry(now);
        }
    }

    (void)raw;
    (void)raw_len;
}

static void ServoP3_Telemetry(uint32_t now)
{
    char line[192];
    int len;

    if (!s_telemetry || (int32_t)(now - s_next_telemetry_tick) < 0) {
        return;
    }
    s_next_telemetry_tick = now + APP_P3_TELEMETRY_PERIOD_MS;

    len = snprintf(line, sizeof(line),
                   "P3 st=%s online=%u id=%u model=0x%04X baud=%u rate=%lu bstep=%u pos=%u ang10=%ld "
                   "mV=%u T=%u cur=%u load=%d move=%u err=%u rx=%lu to=%lu pe=%lu te=%lu\r\n",
                   ServoP3_StateText(s_status.state),
                   (unsigned)s_status.online,
                   (unsigned)s_status.reported_id,
                   (unsigned)s_status.model,
                   (unsigned)s_status.baud_index,
                   (unsigned long)s_status.mcu_baud,
                   (unsigned)s_status.baud_step,
                   (unsigned)s_status.position,
                   (long)s_status.angle_tenths,
                   (unsigned)s_status.voltage_mv,
                   (unsigned)s_status.temperature_c,
                   (unsigned)s_status.current_ma,
                   (int)s_status.load,
                   (unsigned)s_status.moving,
                   (unsigned)s_status.last_error,
                   (unsigned long)s_status.responses,
                   (unsigned long)s_status.timeouts,
                   (unsigned long)s_status.parse_errors,
                   (unsigned long)s_status.tx_errors);

    if ((len > 0) && ((size_t)len < sizeof(line))) {
        if (VOFA_Protocol_SendRaw((const uint8_t *)line, (uint16_t)len) != HAL_OK) {
            s_status.tx_errors++;
        }
    }
}

void ServoP3_Init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    s_servo_id = (uint8_t)BOARD_SERVO_ID;
    s_status.reported_id = s_servo_id;
    s_status.baud_index = (uint8_t)BOARD_SERVO_BAUD_INDEX;
    s_status.mcu_baud = (uint32_t)BOARD_UART_SERVO_BAUD;

    FeetechStsBus_Init();
    ServoP3_ResetProbe(HAL_GetTick());
}

void ServoP3_RequestProbe(void)
{
    s_status.scan_active=0;s_servo_id=BOARD_SERVO_ID;s_status.reported_id=s_servo_id;s_status.baud_locked=0;
    ServoP3_ResetProbe(HAL_GetTick());
}

uint8_t ServoP3_ProbeAt(uint8_t id, uint32_t baud)
{
    if (id >= 254U || baud < 9600U || baud > 1000000U || s_status.state == SERVO_P3_STATE_ONLINE) return 0;
    ServoP3_SetBaud(baud);
    if (s_status.mcu_baud != baud) return 0;
    s_status.scan_active=0;
    s_servo_id = id; s_status.reported_id = id; s_status.baud_locked = 1;
    ServoP3_ResetProbe(HAL_GetTick());
    return 1;
}

/* Read-only unicast discovery, never broadcasts a motion/EEPROM write. */
uint8_t ServoP3_StartScan(uint32_t baud){
 if(!ServoP3_ProbeAt(0U,baud))return 0;
 s_status.scan_active=1;return 1;
}

void ServoP3_Task(uint32_t now_ms)
{
    uint8_t raw[FEETECH_STS_FRAME_MAX_LEN];
    uint8_t raw_len;

    raw_len = 0U;

    switch (s_status.state) {
    case SERVO_P3_STATE_PING:
        if (s_waiting == 0U) {
            if ((int32_t)(now_ms - s_next_action_tick) >= 0) {
                if (FeetechStsBus_Ping(s_servo_id) == HAL_OK) {
                    s_waiting = 1U;
                    s_wait_start_tick = now_ms;
                    s_status.ping_attempts++;
                    s_next_action_tick = now_ms + SERVO_P3_PING_PERIOD_MS;
                } else {
                    s_status.tx_errors++;
                    ServoP3_EnterRetry(now_ms);
                }
            }
        } else if (ServoP3_PollPacket(raw, &raw_len, now_ms) != 0U) {
            s_waiting = 0U;
            s_status.baud_locked = 1U;
            s_status.state = (uint8_t)SERVO_P3_STATE_READ_ID;
            (void)ServoP3_ReadU8(FEETECH_STS_ADDR_ID, now_ms);
        } else if ((uint32_t)(now_ms - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS) {
            s_waiting = 0U;
            s_status.timeouts++;
            if(s_status.scan_active){
                if(s_servo_id<253U){s_servo_id++;s_status.reported_id=s_servo_id;ServoP3_ResetProbe(now_ms);}
                else {s_status.scan_active=0;s_status.state=SERVO_P3_STATE_IDLE;s_status.online=0;}
            } else if (s_status.ping_attempts >= SERVO_P3_PING_ATTEMPTS) {
                if ((s_status.baud_locked == 0U) &&
                    ((uint32_t)(s_status.baud_step + 1U) < (uint32_t)SERVO_P3_BAUD_COUNT)) {
                    s_status.baud_step++;
                    ServoP3_SetBaud(s_baud_candidates[s_status.baud_step]);
                    s_status.ping_attempts = 0U;
                    s_status.state = (uint8_t)SERVO_P3_STATE_PING;
                    s_waiting = 0U;
                    s_next_action_tick = now_ms;
                } else {
                    ServoP3_EnterRetry(now_ms);
                }
            }
        }
        break;

    case SERVO_P3_STATE_READ_ID:
        if (ServoP3_PollPacket(raw, &raw_len, now_ms) != 0U) {
            if (raw_len >= 6U) {
                if(raw[5]!=s_servo_id){s_status.parse_errors++;s_status.scan_active=0;ServoP3_EnterRetry(now_ms);break;}
                s_status.reported_id = raw[5];
                s_status.scan_active=0;
            }
            s_status.state = (uint8_t)SERVO_P3_STATE_READ_MODEL;
            (void)ServoP3_StartRead(FEETECH_STS_ADDR_MODEL_L, 2U, now_ms);
        } else if ((uint32_t)(now_ms - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS) {
            s_status.timeouts++;
            if(s_status.scan_active){
                if(s_servo_id<253U){s_servo_id++;s_status.reported_id=s_servo_id;ServoP3_ResetProbe(now_ms);}
                else {s_status.scan_active=0;s_status.state=SERVO_P3_STATE_IDLE;s_status.online=0;}
            }else ServoP3_EnterRetry(now_ms);
        }
        break;

    case SERVO_P3_STATE_READ_MODEL:
        if (ServoP3_PollPacket(raw, &raw_len, now_ms) != 0U) {
            if (raw_len >= 7U) {
                s_status.model = (uint16_t)((uint16_t)raw[5] | ((uint16_t)raw[6] << 8));
            }
            s_status.state = (uint8_t)SERVO_P3_STATE_READ_BAUD;
            (void)ServoP3_ReadU8(FEETECH_STS_ADDR_BAUD_RATE, now_ms);
        } else if ((uint32_t)(now_ms - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS) {
            s_status.timeouts++;
            ServoP3_EnterRetry(now_ms);
        }
        break;

    case SERVO_P3_STATE_READ_BAUD:
        if (ServoP3_PollPacket(raw, &raw_len, now_ms) != 0U) {
            if (raw_len >= 6U) {
                s_status.baud_index = raw[5];
            }
            s_status.state = (uint8_t)SERVO_P3_STATE_READ_LIMITS;
            (void)ServoP3_StartRead(FEETECH_STS_ADDR_MIN_ANGLE_L, 4U, now_ms);
        } else if ((uint32_t)(now_ms - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS) {
            s_status.timeouts++;
            ServoP3_EnterRetry(now_ms);
        }
        break;

    case SERVO_P3_STATE_READ_LIMITS:
        if (ServoP3_PollPacket(raw, &raw_len, now_ms) != 0U) {
            if (raw_len >= 9U) {
                s_status.min_position = (uint16_t)((uint16_t)raw[5] | ((uint16_t)raw[6] << 8));
                s_status.max_position = (uint16_t)((uint16_t)raw[7] | ((uint16_t)raw[8] << 8));
            }
            s_status.state = (uint8_t)SERVO_P3_STATE_ONLINE;
            s_status.online = 1U;
            s_waiting = 0U;
            s_next_action_tick = now_ms;
        } else if ((uint32_t)(now_ms - s_wait_start_tick) > SERVO_P3_PROBE_TIMEOUT_MS) {
            s_status.timeouts++;
            ServoP3_EnterRetry(now_ms);
        }
        break;

    case SERVO_P3_STATE_ONLINE:
        ServoP3_TaskFeedback(now_ms, raw, &raw_len);
        break;

    case SERVO_P3_STATE_IDLE: break;
    case SERVO_P3_STATE_RETRY:
    default:
        if ((int32_t)(now_ms - s_next_action_tick) >= 0) {
            ServoP3_ResetProbe(now_ms);
        }
        break;
    }

    ServoP3_Telemetry(now_ms);
}

const ServoP3Status *ServoP3_GetStatus(void)
{
    return &s_status;
}
