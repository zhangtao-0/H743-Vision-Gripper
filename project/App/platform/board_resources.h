#ifndef BOARD_RESOURCES_H
#define BOARD_RESOURCES_H
/* HAL-only internals; business code should include board_uart.h instead. */
#include "platform_hal.h"
#include "board_uart.h"
UART_HandleTypeDef *BoardResource_GetUart(BoardUartRole role);
BoardStatus BoardUart_HardwareMspInit(UART_HandleTypeDef *uart);
void BoardUart_HardwareMspDeInit(UART_HandleTypeDef *uart);
void BoardUart_Irq(BoardUartRole role);
#endif
