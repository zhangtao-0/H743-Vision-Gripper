/*********************************************************************************************************************
 * @file    uart_router.c
 * @brief   HAL UART 回调的唯一分发入口。
 *
 * @details
 * 1. 按 BoardUartRole 分发，不按 USARTx 实例硬编码。
 * 2. 回调只做“搬运/重新挂接收”，协议解析在任务上下文完成。
 * 3. bsp_bluetooth.h 中的 BLUETOOTH_ENABLE_HAL_UART_CALLBACK 必须保持 0，
 *    否则会与本文件重复定义 HAL 回调。
 *********************************************************************************************************************/

#include "board_uart.h"
#include "feature_config.h"
#include "host_diagnostics.h"
#include "bsp_bluetooth.h"
#include "feetech_sts_bus.h"
#include "vofa_protocol.h"

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (BoardUart_IsInstance(huart, BOARD_UART_SERVO) != 0U) {
        FeetechStsBus_HandleRxComplete();
    } else if (BoardUart_IsInstance(huart, BOARD_UART_HOST) != 0U) {
#if APP_HOST_DIAGNOSTICS_ENABLE
        HostDiagnostics_RxComplete();
#else
        VOFA_Protocol_HandleRxComplete();
#endif
    } else if (BoardUart_IsInstance(huart, BOARD_UART_BLUETOOTH) != 0U) {
        Bluetooth_RxCpltCallback(huart);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (BoardUart_IsInstance(huart, BOARD_UART_HOST) != 0U) {
#if !APP_HOST_DIAGNOSTICS_ENABLE
        VOFA_Protocol_TxCpltCallback(huart);
#endif
    } else if (BoardUart_IsInstance(huart, BOARD_UART_BLUETOOTH) != 0U) {
        Bluetooth_TxCpltCallback(huart);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (BoardUart_IsInstance(huart, BOARD_UART_SERVO) != 0U) {
        (void)FeetechStsBus_StartReceive();
    } else if (BoardUart_IsInstance(huart, BOARD_UART_HOST) != 0U) {
#if APP_HOST_DIAGNOSTICS_ENABLE
        HostDiagnostics_Error();
#else
        VOFA_Protocol_ErrorCallback(huart);
#endif
    } else if (BoardUart_IsInstance(huart, BOARD_UART_BLUETOOTH) != 0U) {
        Bluetooth_ErrorCallback(huart);
    }
}
