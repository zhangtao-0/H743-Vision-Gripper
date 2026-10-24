/*********************************************************************************************************************
 * @file    vofa_protocol.h
 * @brief   VOFA/USB-TTL 协议接口。
 *
 * @details
 * 1. 本文件负责：VOFA 收发、帧缓存和状态查询接口。
 * 2. 主要流程：VOFA 收包 -> 上层读取并转发。
 *********************************************************************************************************************/

#ifndef __VOFA_PROTOCOL_H__
#define __VOFA_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "board_uart.h"
#include "feetech_sts_bus.h"
#include "claw_host_protocol.h"
#include <stdint.h>

/* 角色 → 句柄的唯一绑定在 board_resources.c；这里只取引用。 */

#define VOFA_FEETECH_FRAME_MAX_LEN  FEETECH_STS_FRAME_MAX_LEN
#define VOFA_RX_FRAME_QUEUE_DEPTH   8U    // 保存连续的按下/松开帧。
#define VOFA_TX_BUFFER_SIZE         1024U /* full multi-line @status burst + telemetry */
#define VOFA_TX_CHUNK_MAX_LEN       64U

#define VOFA_DEBUG_ECHO_RX_BYTE                  0U          /*调试变量： 1：每收到 1 字节立即回显。 */

typedef struct {
    uint8_t  rx_active;              // 接收：UART 单字节中断接收已挂起
    uint8_t  last_byte;              // 接收：最近收到的 1 个原始字节
    uint8_t  frame_ready;            // 接收：完整帧待读取标志
    uint8_t  frame_len;              // 接收：完整帧长度，包含 FF FF 和 CheckSum
    uint8_t frame_queue_count;    // 等待处理的完整帧数量。
    uint32_t last_rx_tick;           // 接收：最近接收时间戳，单位 ms
    uint32_t rx_count;               // 接收：已接收字节总数
    uint32_t rx_overflow_count;      // 接收：接收缓存溢出次数
    
    uint8_t  tx_busy;                // 发送：UART 中断发送尚未完成
    uint16_t tx_count_in_buf;        // 发送：发送缓存中待发送字节数
    uint32_t tx_count;               // 发送：已完成发送字节总数
    uint32_t tx_overflow_count;      // 发送：发送缓存溢出次数

    uint32_t frame_count;            // 组帧：校验通过的完整飞特帧数量
    uint32_t frame_error_count;      // 组帧：帧头/长度/校验失败次数

    uint32_t uart_error_count;       // UART：硬件错误次数
} VOFA_Status_t;

void VOFA_Protocol_Init(void);
void VOFA_Protocol_Task(void);                                           // VOFA 周期任务：处理接收缓存 -> 解析完整飞特帧 -> 转发给命令层。
void VOFA_Protocol_RxCpltCallback(UART_HandleTypeDef *huart);         //接收完成回调
void VOFA_Protocol_TxCpltCallback(UART_HandleTypeDef *huart);         //发送完成回调
void VOFA_Protocol_ErrorCallback(UART_HandleTypeDef *huart);          //错误回调
void VOFA_Protocol_HandleRxComplete(void);                              //接收完成处理：UART 字节 -> VOFA 接收状态/飞特帧缓存。

uint8_t VOFA_Protocol_ReadFeetechFrame(uint8_t *buffer, uint8_t buffer_size, uint8_t *out_len);
void VOFA_Protocol_ClearFeetechFrame(void);                                                         // 清除缓存飞特帧
HAL_StatusTypeDef VOFA_Protocol_SendFeetechFrame(const uint8_t *frame, uint8_t len);              // 发送完整飞特帧：校验飞特帧 -> VOFA 发送缓存 -> UART 发送。
HAL_StatusTypeDef VOFA_Protocol_SendRaw(const uint8_t *data, uint16_t len);                         // 发送原始字节流：不校验协议 -> 直接进入 VOFA 发送缓存。
uint8_t VOFA_Protocol_SetPlotExclusive(uint8_t on);
HAL_StatusTypeDef VOFA_Protocol_SendPlot(const uint8_t *data,uint16_t len);
const VOFA_Status_t *VOFA_Protocol_GetStatus(void);                                              // 获取 VOFA 状态

#ifdef __cplusplus
}
#endif

#endif
