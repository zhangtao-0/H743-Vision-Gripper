/*********************************************************************************************************************
 * @file    board_uart.c
 * @brief   角色化 UART 字节收发实现。
 *
 * @details
 * 1. 通过 BoardResource_GetUart(role) 取句柄，本文件不 extern huart1/2/3。
 * 2. 只做字节级收发/溢出清除，不解析任何协议。
 * 3. 中断回调由 uart_router.c 统一分发。
 *********************************************************************************************************************/

#include "board_uart.h"
#include "board_config.h"
#include "board_resources.h"

static UART_HandleTypeDef *BoardUart_Handle(BoardUartRole role)
{
    return BoardResource_GetUart(role);
}

static BoardStatus BoardUart_Status(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK: return BOARD_OK;
        case HAL_BUSY: return BOARD_BUSY;
        case HAL_TIMEOUT: return BOARD_TIMEOUT;
        default: return BOARD_ERROR;
    }
}

BoardStatus BoardUart_InitAll(void)
{
    uint32_t i;
    uint32_t j;
    static const uint32_t baud[BOARD_UART_COUNT] = {
#define UART_BAUD(role,handle) [BOARD_UART_##role]=BOARD_UART_##role##_BAUD,
        BOARD_UART_BINDINGS(UART_BAUD)
#undef UART_BAUD
    };

    for (i = 0U; i < (uint32_t)BOARD_UART_COUNT; i++) {
        UART_HandleTypeDef *uart = BoardUart_Handle((BoardUartRole)i);
        if ((uart == 0) || (uart->Instance == 0) || (uart->Init.BaudRate != baud[i])) {
            return BOARD_ERROR;
        }
        for (j = 0U; j < i; j++) {
            if (uart->Instance == BoardUart_Handle((BoardUartRole)j)->Instance) {
                return BOARD_ERROR;
            }
        }
    }
    return BOARD_OK;
}

BoardStatus BoardUart_Transmit(
    BoardUartRole role,
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);

    if ((uart == 0) || (data == 0) || (len == 0U)) {
        return BOARD_ERROR;
    }

    return BoardUart_Status(HAL_UART_Transmit(uart, (uint8_t *)data, len, timeout_ms));
}

BoardStatus BoardUart_TransmitIT(BoardUartRole role,const uint8_t *data,uint16_t len)
{
 UART_HandleTypeDef *uart=BoardUart_Handle(role);
 if(!uart||!data||!len)return BOARD_ERROR;
 return BoardUart_Status(HAL_UART_Transmit_IT(uart,(uint8_t *)data,len));
}

BoardStatus BoardUart_SetBaud(BoardUartRole role, uint32_t baud)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);

    if ((uart == 0) || (baud == 0U)) {
        return BOARD_ERROR;
    }

    if (HAL_UART_AbortReceive(uart) != HAL_OK) return BOARD_ERROR;
    uart->Init.BaudRate = baud;
    return (HAL_UART_Init(uart) == HAL_OK) ? BOARD_OK : BOARD_ERROR;
}

uint32_t BoardUart_GetBaud(BoardUartRole role)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);
    return (uart != 0) ? uart->Init.BaudRate : 0U;
}

BoardStatus BoardUart_StartRxIT(BoardUartRole role, uint8_t *byte)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);

    if ((uart == 0) || (byte == 0)) {
        return BOARD_ERROR;
    }

    if (uart->RxState != HAL_UART_STATE_READY) {
        return BOARD_BUSY;
    }

    __HAL_UART_CLEAR_OREFLAG(uart);

    return BoardUart_Status(HAL_UART_Receive_IT(uart, byte, 1U));
}

void BoardUart_ClearOverrun(BoardUartRole role)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);

    if (uart != 0) {
        __HAL_UART_CLEAR_OREFLAG(uart);
    }
}

uint8_t BoardUart_IsInstance(const void *instance, BoardUartRole role)
{
    UART_HandleTypeDef *uart = BoardUart_Handle(role);

    if ((instance == 0) || (uart == 0)) {
        return 0U;
    }

    return (instance == (const void *)uart) ? 1U : 0U;
}

/* Byte-level host regressions may omit MCU hardware setup; production always includes it. */
#ifndef PLATFORM_TEST_UART_BYTE_ONLY
#include PLATFORM_UART_HW_BACKEND
#endif
