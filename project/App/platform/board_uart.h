/*********************************************************************************************************************
 * @file    board_uart.h
 * @brief   角色化 UART 字节收发接口。
 *
 * @details
 * 1. 调用者只传 BoardUartRole，不接触 UART_HandleTypeDef。
 * 2. 具体句柄由 board_resources.c 的绑定表提供。
 * 3. 返回值使用 BoardStatus，避免 HAL 类型继续向上层扩散。
 *********************************************************************************************************************/

#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdint.h>
typedef enum {
 BOARD_UART_SERVO=0, BOARD_UART_HOST, BOARD_UART_BLUETOOTH, BOARD_UART_COUNT
} BoardUartRole;

typedef enum {
    BOARD_OK = 0,
    BOARD_BUSY,
    BOARD_ERROR,
    BOARD_TIMEOUT
} BoardStatus;

/* Validate configured and initialized role bindings. */
BoardStatus BoardUart_ConfigureHardware(void);
BoardStatus BoardUart_InitAll(void);
BoardStatus BoardUart_TransmitIT(BoardUartRole role,const uint8_t *data,uint16_t len);

BoardStatus BoardUart_Transmit(
    BoardUartRole role,
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms);

/* 运行期改波特率：用于 P3 只读探测的波特率回退（不改舵机 EEPROM）。 */
BoardStatus BoardUart_SetBaud(BoardUartRole role, uint32_t baud);

/* 读取角色当前波特率；用于按实际线速计算发送预算。 */
uint32_t BoardUart_GetBaud(BoardUartRole role);

BoardStatus BoardUart_StartRxIT(
    BoardUartRole role,
    uint8_t *byte);

void BoardUart_ClearOverrun(BoardUartRole role);

/* 判断某个 HAL 句柄是否属于指定角色。 */
uint8_t BoardUart_IsInstance(const void *instance, BoardUartRole role);

#endif /* BOARD_UART_H */
