/* P2 专用二进制回显；不解析指令、不访问舵机，发送只在主循环。 */
#include "host_diagnostics.h"
#include "board_uart.h"

#define RX_SIZE 256U
static uint8_t s_rx_byte, s_rx[RX_SIZE];
static volatile uint16_t s_head, s_tail;
static HostDiagnosticsStatus s_status;

static void Rearm(void)
{
    BoardStatus status = BoardUart_StartRxIT(BOARD_UART_HOST, &s_rx_byte);
    if (status != BOARD_OK && status != BOARD_BUSY) s_status.rearm_errors++;
}

void HostDiagnostics_Init(void)
{
    s_head = s_tail = 0U;
    s_status.rx_bytes = s_status.tx_bytes = s_status.rx_overflow = 0U;
    s_status.uart_errors = s_status.tx_errors = s_status.rearm_errors = 0U;
    Rearm();
}

void HostDiagnostics_RxComplete(void)
{
    uint16_t next = (uint16_t)((s_head + 1U) % RX_SIZE);
    s_status.rx_bytes++;
    if (next == s_tail) s_status.rx_overflow++;
    else {
        s_rx[s_head] = s_rx_byte;
        __DMB();
        s_head = next;
    }
    Rearm();
}

void HostDiagnostics_Error(void)
{
    s_status.uart_errors++;
    BoardUart_ClearOverrun(BOARD_UART_HOST);
    /* H7 HAL 的阻塞接收错误会先终止 RX，再调用此处；主循环也会重试。 */
    Rearm();
}

void HostDiagnostics_Task(void)
{
    uint8_t chunk[32];
    uint16_t n = 0U, cursor = s_tail;
    uint16_t head = s_head;
    __DMB();
    while (cursor != head && n < sizeof(chunk)) {
        chunk[n++] = s_rx[cursor];
        cursor = (uint16_t)((cursor + 1U) % RX_SIZE);
    }
    if (n != 0U) {
        BoardStatus status = BoardUart_Transmit(BOARD_UART_HOST, chunk, n, 10U);
        if (status == BOARD_OK) {
            s_status.tx_bytes += n;
            __DMB();
            s_tail = cursor;
        } else if (status != BOARD_BUSY) {
            /* 错误时丢弃本批并计数；避免部分发送后重试制造重复回显。 */
            s_status.tx_errors++;
            s_tail = cursor;
        }
    }
    Rearm();
}

const HostDiagnosticsStatus *HostDiagnostics_GetStatus(void) { return &s_status; }
