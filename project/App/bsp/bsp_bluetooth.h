/*********************************************************************************************************************
 * @file    bsp_bluetooth.h
 * @brief   蓝牙 BSP 对外接口。
 *
 * @details
 * 1. 本文件负责：声明蓝牙收发接口、缓存结构和调试状态。
 * 2. 本文件不负责：暴露上层动作细节。
 * 3. 主要流程：上层调用 -> 读取缓存/写入发送队列。
 *********************************************************************************************************************/

#ifndef __BSP_BLUETOOTH_H
#define __BSP_BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "board_uart.h"
#include "feature_config.h"
#include "claw_host_protocol.h"
#include "feetech_sts_bus.h"
#include <stdint.h>

/* 角色 → 句柄的唯一绑定在 board_resources.c；这里只取引用。 */

#define BLUETOOTH_ENABLE_HAL_UART_CALLBACK      0U          /* 0：由 uart_router.c 统一分发 HAL 回调。 */

#define BLUETOOTH_TRANSPORT_WRAPPED     0U      // 收发：A5 + 飞特帧 + 5A
#define BLUETOOTH_TRANSPORT_RAW         1U      // 收发：裸飞特帧
#define BLUETOOTH_TRANSPORT_AUTO        2U      // 接收两种格式，发送固定外层包
#define BLUETOOTH_TRANSPORT_MODE        BLUETOOTH_TRANSPORT_RAW

#if ((BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_WRAPPED) && \
     (BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_RAW) && \
     (BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_AUTO))
#error "Invalid BLUETOOTH_TRANSPORT_MODE"
#endif

#define BLUETOOTH_APP_HEAD                      0xA5U       /* 兼容旧外层包。 */
#define BLUETOOTH_APP_TAIL                      0x5AU       /* 兼容旧外层包。 */
#define BLUETOOTH_FEETECH_FRAME_MAX_LEN         FEETECH_STS_FRAME_MAX_LEN
#define BLUETOOTH_RX_FRAME_QUEUE_DEPTH          CLAW_BLUETOOTH_RX_FRAME_QUEUE_DEPTH
#define BLUETOOTH_TX_BUFFER_SIZE                CLAW_BLUETOOTH_TX_BUFFER_SIZE
#define BLUETOOTH_TX_CHUNK_MAX_LEN              CLAW_BLUETOOTH_TX_CHUNK_MAX_LEN
#define BLUETOOTH_RX_PACKET_TIMEOUT_MS          CLAW_BLUETOOTH_RX_PACKET_TIMEOUT_MS
#define BLUETOOTH_TX_MAX_LEN                    BLUETOOTH_TX_BUFFER_SIZE

#define BLUETOOTH_SLIDER_VALUE_ASCII            0U          /* 十进制 ASCII：0~100。 */
#define BLUETOOTH_SLIDER_VALUE_U8               1U          /* 1 字节无符号整数。 */
#define BLUETOOTH_SLIDER_VALUE_U16_LE           2U          /* 2 字节小端整数。 */
#define BLUETOOTH_SLIDER_VALUE_U32_LE           3U          /* 4 字节小端整数。 */
#define BLUETOOTH_SLIDER_ENABLE                 CLAW_FEATURE_BLUETOOTH_SLIDER_ENABLE
#define BLUETOOTH_SLIDER_HEAD                   CLAW_DEFAULT_BLUETOOTH_SLIDER_HEAD
#define BLUETOOTH_SLIDER_TAIL                   CLAW_DEFAULT_BLUETOOTH_SLIDER_TAIL
#define BLUETOOTH_SLIDER_VALUE_FORMAT           CLAW_DEFAULT_BLUETOOTH_SLIDER_FORMAT
#define BLUETOOTH_SLIDER_CHANNEL_ANGLE          CLAW_DEFAULT_BLUETOOTH_ANGLE_CHANNEL
#define BLUETOOTH_SLIDER_PAYLOAD_MAX_LEN        4U
#define BLUETOOTH_SLIDER_VALUE_MAX              100U

#if (BLUETOOTH_SLIDER_VALUE_FORMAT > BLUETOOTH_SLIDER_VALUE_U32_LE)
#error "Invalid BLUETOOTH_SLIDER_VALUE_FORMAT"
#endif

#define BLUETOOTH_UPDATE_NONE                   0x00U
#define BLUETOOTH_UPDATE_FEETECH_FRAME          0x01U       /* 收到一帧官方/F5 帧。 */
#define BLUETOOTH_UPDATE_SLIDER                  0x02U       /* 收到一包滑块整数。 */

typedef enum {
    BSP_BT_OK = 0,        // 操作成功
    BSP_BT_ERROR,         // 通用错误
    BSP_BT_BUSY,          // 蓝牙串口忙，当前不能发送/接收
    BSP_BT_BAD_PARAM,     // 参数错误，如空指针、长度为 0、长度超限
    BSP_BT_OVERFLOW,      // 缓冲区溢出，收到的数据超过缓存长度
    BSP_BT_BAD_FRAME      // 飞特帧错误，如帧头、长度或校验失败
} BSP_BT_Status_t;

typedef struct {
    uint8_t channel;                             // 滑块通道。
    uint8_t percent;                             // 0~100 整数值。
} BluetoothSliderEvent_t;

typedef struct {
    uint8_t  initialized;                        // 模块初始化标志

    //接收状态：UART 单字节 -> 飞特帧解析 -> 完整帧缓存。
    uint8_t  rx_active;                          // UART 单字节中断接收已挂起
    uint32_t rx_count;                           // 字节总数
    uint8_t  last_byte;                          // 最近收到的 1 个字节
    uint8_t  frame_ready;                        // 完整飞特帧待读取
    uint8_t  frame_len;                          // 队首帧长度
    uint8_t  frame_queue_count;                  // 等待上层处理的完整帧数量
    uint32_t frame_count;                        // 校验完整数量
    uint32_t frame_error_count;                  // 官方/F5 帧错误次数
    uint8_t  slider_ready;                       // 滑块事件待读取
    uint8_t  slider_channel;                     // 最近滑块通道
    uint8_t  slider_percent;                     // 最近滑块整数
    uint32_t slider_count;                       // 有效滑块包数量
    uint32_t slider_error_count;                 // 滑块包错误次数
    uint32_t overflow_count;                     // 接收溢出次数
    uint32_t uart_error_count;                   // UART 硬件错误次数
    uint32_t last_rx_tick;                       // 最近接收时间戳， ms

    // 发送状态：tx_buf 写入 -> 等待 UART 空闲 -> 中断发送。
    uint8_t  tx_pending;                         // 发送队列非空
    uint8_t  tx_busy;                            // 硬件发送中
    uint16_t tx_len;                             // 队列待发送字节数
    uint32_t tx_count;                           // 已发送字节数
    uint32_t tx_overflow_count;                  // 发送队列空间不足次数
} BluetoothStatus_t;

void Bluetooth_Init(void);
void Bluetooth_Task(void);                             // 主循环维护：接收恢复、超时复位、发送推进。

void Bluetooth_TimerIRQHandler(void);                  // 兼容旧入口，内部调用 Bluetooth_Task。
void Bluetooth_RxCpltCallback(UART_HandleTypeDef *huart);  // 接收完成回调：收到 1 字节 -> 组帧 -> 继续接收
void Bluetooth_TxCpltCallback(UART_HandleTypeDef *huart);  // 发送完成回调：清发送忙标志 -> 尝试发送下一包
void Bluetooth_ErrorCallback(UART_HandleTypeDef *huart);   // 错误回调：统计错误 -> 复位解析器 -> 重启接收

uint8_t Bluetooth_ReadFeetechFrame(uint8_t *buf, uint8_t buf_size, uint8_t *len);
uint8_t Bluetooth_ReadSliderEvent(BluetoothSliderEvent_t *event);                           // 读取滑块通道和值。
void Bluetooth_ClearFeetechFrame(void);                                                     // 清除缓存帧
BSP_BT_Status_t Bluetooth_SendFeetechFrame(const uint8_t *frame, uint8_t len);              // 发送完整帧
BSP_BT_Status_t Bluetooth_SendFeetechCommand(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len);        // 组帧并发送
const BluetoothStatus_t *Bluetooth_GetStatus(void);                                         // 获取蓝牙状态

/* 蓝牙旧接口名称保留 */
uint8_t Bluetooth_ReadPacket(uint8_t *buf, uint8_t buf_size, uint8_t *len);        //读取
void Bluetooth_ClearPacket(void);                                                 //清除
BSP_BT_Status_t Bluetooth_SendPacket(const uint8_t *frame, uint8_t len);          //发送包
BSP_BT_Status_t Bluetooth_SendRaw(const uint8_t *data, uint16_t len);             //发送任意原始字节
BSP_BT_Status_t Bluetooth_SendTextLine(const char *text);                         //发送文本并追加换行

#ifdef __cplusplus
}
#endif

#endif
