
#include "feetech_sts_port.h"
#include "board_uart.h"
#include "board_config.h"

static HAL_StatusTypeDef FeetechStsPort_Status(BoardStatus status)
{
    switch (status) {
        case BOARD_OK: return HAL_OK;
        case BOARD_BUSY: return HAL_BUSY;
        case BOARD_TIMEOUT: return HAL_TIMEOUT;
        default: return HAL_ERROR;
    }
}

/*
 * STS3215 总线通过板级角色 BOARD_UART_SERVO 访问。
 * 具体是 USART2 还是其他串口，由 board_resources.c 的唯一绑定表决定。
 */

HAL_StatusTypeDef FeetechStsPort_StartReceiveIT(uint8_t *data)
{
    return FeetechStsPort_Status(BoardUart_StartRxIT(BOARD_UART_SERVO, data));
}

HAL_StatusTypeDef FeetechStsPort_Transmit(
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms)
{
    /* 8N1: 每字节 10 bit；按当前实际波特率覆盖长帧线速时间。 */
    uint32_t baud = BoardUart_GetBaud(BOARD_UART_SERVO);
    uint32_t wire_ms;

    if (baud == 0U) {
        baud = BOARD_UART_SERVO_BAUD;
    }
    wire_ms = ((uint32_t)len * 10000U + baud - 1U) / baud;
    if (timeout_ms < wire_ms + 20U) {
        timeout_ms = wire_ms + 20U;
    }
    return FeetechStsPort_Status(BoardUart_Transmit(BOARD_UART_SERVO, data, len, timeout_ms));
}

void FeetechStsPort_ClearOverrun(void)
{
    BoardUart_ClearOverrun(BOARD_UART_SERVO);
}

uint8_t FeetechStsPort_IsUart(
    const UART_HandleTypeDef *huart)
{
    return BoardUart_IsInstance(huart, BOARD_UART_SERVO);
}
