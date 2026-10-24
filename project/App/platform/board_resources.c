#include "board_resources.h"
#include "board_config.h"
#define UART_EXTERN(role,handle) extern UART_HandleTypeDef handle;
BOARD_UART_BINDINGS(UART_EXTERN)
#undef UART_EXTERN
#define UART_BIND(role,handle) [BOARD_UART_##role] = &handle,
static UART_HandleTypeDef *const s_uart[BOARD_UART_COUNT]={BOARD_UART_BINDINGS(UART_BIND)};
#undef UART_BIND
UART_HandleTypeDef *BoardResource_GetUart(BoardUartRole role)
{return (unsigned)role<BOARD_UART_COUNT?s_uart[role]:0;}
