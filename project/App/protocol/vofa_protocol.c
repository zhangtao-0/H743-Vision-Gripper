/*********************************************************************************************************************
 * @file    vofa_protocol.c
 * @brief   VOFA/USB-TTL 协议收发实现。
 *
 * @details
 * 1. 本文件负责：管理 USART3 字节接收、飞特帧缓存和发送队列。
 * 2. 本文件不负责：直接控制机械爪动作。
 * 3. 主要流程：USB 字节 -> 飞特帧 -> command 层转发。
 *********************************************************************************************************************/

#include "vofa_protocol.h"
#include "claw_host_protocol.h"
#include "integration_console.h"
#include <string.h>

typedef enum {
    VOFA_PARSE_WAIT_FF1 = 0,                       /* 等待第 1 个 0xFF。 */
    VOFA_PARSE_WAIT_FF2,                           /* 等待第 2 个 0xFF。 */
    VOFA_PARSE_READ_FRAME                          /* 正在读取完整飞特帧。 */
} VOFA_ParseState_t;

typedef struct {
    volatile uint8_t frame_queue
        [VOFA_RX_FRAME_QUEUE_DEPTH]
        [VOFA_FEETECH_FRAME_MAX_LEN];

    volatile uint8_t frame_len_queue
        [VOFA_RX_FRAME_QUEUE_DEPTH];

    volatile uint8_t frame_head;          // 下一帧写入位置。
    volatile uint8_t frame_tail;          // 下一帧读取位置。
    volatile uint8_t frame_queue_count;   // 当前帧数量。
    volatile uint8_t rx_byte;
    volatile uint8_t rx_active;
    volatile uint8_t last_byte;
    volatile uint8_t tx_buf[VOFA_TX_BUFFER_SIZE];
    volatile uint16_t tx_count_in_buf;
    volatile uint8_t tx_busy;
    volatile uint32_t rx_count;
    volatile uint32_t tx_count;
    volatile uint32_t frame_count;
    volatile uint32_t frame_error_count;
    volatile uint32_t rx_overflow_count;
    volatile uint32_t tx_overflow_count;
    volatile uint32_t uart_error_count;
    volatile uint32_t last_rx_tick;
} VOFA_Runtime_t;

typedef struct {
    VOFA_ParseState_t state;
    uint8_t frame[VOFA_FEETECH_FRAME_MAX_LEN];
    uint8_t index;
    uint8_t expected_len;
} VOFA_RxParser_t;

typedef struct {
    uint16_t head;
    uint16_t tail;
    uint8_t active_buf[VOFA_TX_CHUNK_MAX_LEN];
    uint16_t active_len;
} VOFA_TxRuntime_t;

static VOFA_Runtime_t s_vofa;
static VOFA_RxParser_t s_rx;
static VOFA_TxRuntime_t s_tx;
static VOFA_Status_t s_status;
static uint8_t s_plot_exclusive;

static void VOFA_ResetParser(void);
static void VOFA_ProcessByte(uint8_t data);
static void VOFA_SaveFrame(const uint8_t *frame, uint8_t len);
static HAL_StatusTypeDef VOFA_StartReceiveIT(void);
static HAL_StatusTypeDef VOFA_TryStartTx(void);
static HAL_StatusTypeDef VOFA_WriteTxBuffer(const uint8_t *data, uint16_t len);
static uint16_t VOFA_TxFreeSizeUnsafe(void);
static void VOFA_UpdateStatus(void);

//初始化 VOFA 收发状态。
void VOFA_Protocol_Init(void)
{
    s_plot_exclusive=0;
    /* 初始化 VOFA 收发缓存和中断接收。 */
    __disable_irq();
    memset((void *)&s_vofa, 0, sizeof(s_vofa));
    memset(&s_rx, 0, sizeof(s_rx));
    memset(&s_tx, 0, sizeof(s_tx));
    memset(&s_status, 0, sizeof(s_status));
    VOFA_ResetParser();
    __enable_irq();

    (void)VOFA_StartReceiveIT();
}

//推进 VOFA 接收和发送。
void VOFA_Protocol_Task(void)
{
    /* USB-TTL 边界：只维护收发缓存，不执行舵机动作。 */
    if (s_vofa.rx_active == 0U) {
        (void)VOFA_StartReceiveIT();
    }
    (void)VOFA_TryStartTx();
    VOFA_UpdateStatus();
}

//VOFA 接收完成 -> 解析字节。
void VOFA_Protocol_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /* VOFA UART 接收完成回调入口。 */
    if (BoardUart_IsInstance(huart,BOARD_UART_HOST)) {
        VOFA_Protocol_HandleRxComplete();
    }
}

//VOFA 发送完成 -> 继续发送队列。
void VOFA_Protocol_TxCpltCallback(UART_HandleTypeDef *huart)
{
    /* VOFA UART 发送完成后释放 busy。 */
    if (BoardUart_IsInstance(huart,BOARD_UART_HOST)) {
        __disable_irq();
        s_vofa.tx_busy = 0U;
        s_vofa.tx_count += s_tx.active_len;
        if (s_vofa.tx_count_in_buf >= s_tx.active_len) {
            s_vofa.tx_count_in_buf = (uint16_t)(s_vofa.tx_count_in_buf - s_tx.active_len);
        } else {
            s_vofa.tx_count_in_buf = 0U;
        }
        s_tx.active_len = 0U;
        __enable_irq();

        (void)VOFA_TryStartTx();
    }
}

//VOFA 串口错误 -> 重启接收。
void VOFA_Protocol_ErrorCallback(UART_HandleTypeDef *huart)
{
    /* 记录 UART 错误并重启接收链路。 */
    if (BoardUart_IsInstance(huart,BOARD_UART_HOST)) {
        s_vofa.uart_error_count++;
        s_vofa.rx_active = 0U;
        s_vofa.tx_busy = 0U;
        VOFA_ResetParser();
            (void)VOFA_StartReceiveIT();
    }
}

//兼容入口，处理 VOFA 接收完成。
void VOFA_Protocol_HandleRxComplete(void)
{
    /* RX 中断流：单字节 -> 帧解析器 -> ready 缓存。 */
    uint8_t data;

    data = s_vofa.rx_byte;

    s_vofa.last_byte = data;
    s_vofa.rx_count++;
    s_vofa.last_rx_tick = HAL_GetTick();

#if (VOFA_DEBUG_ECHO_RX_BYTE != 0U)
    (void)VOFA_Protocol_SendRaw(&data, 1U);
#endif

    if (!IntegrationConsole_PushByte(data, 0U, s_rx.state == VOFA_PARSE_WAIT_FF1)) VOFA_ProcessByte(data);

    s_vofa.rx_active = 0U;
    (void)VOFA_StartReceiveIT();
}

// 读取最早收到的一帧，保持A0/B0按下和松开的先后顺序。
uint8_t VOFA_Protocol_ReadFeetechFrame(
    uint8_t *buffer,
    uint8_t buffer_size,
    uint8_t *out_len)
{
    uint8_t copy_len;
    uint8_t tail;

    if ((buffer == 0) ||
        (out_len == 0) ||
        (buffer_size == 0U)) {
        return 0U;
    }

    __disable_irq();

    if (s_vofa.frame_queue_count == 0U) {
        *out_len = 0U;
        __enable_irq();
        return 0U;
    }

    tail = s_vofa.frame_tail;
    copy_len = s_vofa.frame_len_queue[tail];

    if (copy_len > buffer_size) {
        copy_len = buffer_size;
    }

    memcpy(
        buffer,
        (const void *)s_vofa.frame_queue[tail],
        copy_len);

    *out_len = copy_len;

    s_vofa.frame_tail++;
    if (s_vofa.frame_tail >=
        VOFA_RX_FRAME_QUEUE_DEPTH) {
        s_vofa.frame_tail = 0U;
    }

    s_vofa.frame_queue_count--;

    __enable_irq();
    return 1U;
}

// 清空全部等待处理的VOFA帧。
void VOFA_Protocol_ClearFeetechFrame(void)
{
    __disable_irq();

    s_vofa.frame_head = 0U;
    s_vofa.frame_tail = 0U;
    s_vofa.frame_queue_count = 0U;

    __enable_irq();
}

//发送完整飞特帧：校验飞特帧 -> VOFA 发送缓存 -> UART 发送。
HAL_StatusTypeDef VOFA_Protocol_SendFeetechFrame(const uint8_t *frame, uint8_t len)
{
    /* 发送一帧官方飞特协议数据。 */
    if ((frame == 0) || (len == 0U)) {
        return HAL_ERROR;
    }
    if ((FeetechSts_IsValidFrame(frame, len) == 0U) &&
        (ClawHostProtocol_IsFrame(frame, len) == 0U)) {
        s_vofa.frame_error_count++;
        return HAL_ERROR;
    }

    return VOFA_Protocol_SendRaw(frame, len);
}

// 发送原始字节流：不校验协议 -> 直接进入 VOFA 发送缓存。
static HAL_StatusTypeDef VOFA_SendQueued(const uint8_t *data, uint16_t len)
{
    /* TX 串行化：busy 保护 -> DMA 发送。 */
    HAL_StatusTypeDef status;

    if ((data == 0) || (len == 0U)) {
        return HAL_OK;
    }

    __disable_irq();
    status = VOFA_WriteTxBuffer(data, len);
    __enable_irq();

    if (status == HAL_OK) {
        (void)VOFA_TryStartTx();
    }

    return status;
}

/* No text/binary replies may corrupt a JustFloat plot. USB replies remain live. */
uint8_t VOFA_Protocol_SetPlotExclusive(uint8_t on)
{
    uint32_t lock=__get_PRIMASK();__disable_irq();
    if(on && !s_plot_exclusive && (s_vofa.tx_busy || s_vofa.tx_count_in_buf)){
        __set_PRIMASK(lock);return 0;
    }
    s_plot_exclusive=(on!=0);__set_PRIMASK(lock);return 1;
}
HAL_StatusTypeDef VOFA_Protocol_SendRaw(const uint8_t *data,uint16_t len)
{
    if(s_plot_exclusive)return HAL_BUSY;
    return VOFA_SendQueued(data,len);
}
HAL_StatusTypeDef VOFA_Protocol_SendPlot(const uint8_t *data,uint16_t len)
{
    if(!s_plot_exclusive)return HAL_ERROR;
    return VOFA_SendQueued(data,len);
}

const VOFA_Status_t *VOFA_Protocol_GetStatus(void)
{
    VOFA_UpdateStatus();
    return &s_status;
}

//重置 VOFA 帧解析状态。
static void VOFA_ResetParser(void)
{
    /* 重置 VOFA 帧解析状态。 */
    s_rx.state = VOFA_PARSE_WAIT_FF1;
    s_rx.index = 0U;
    s_rx.expected_len = 0U;
}

//VOFA 字节流 -> 飞特帧。
static void VOFA_ProcessByte(uint8_t data)
{
    /*
     * 注意：解析器不依赖 \r\n。
     * 如果 VOFA 发送时自动追加 \r\n，完整飞特帧结束后，0D/0A 会在 WAIT_FF1 状态下被忽略。
     */
    switch (s_rx.state) {
        case VOFA_PARSE_WAIT_FF1:
            if (data == FEETECH_STS_FRAME_HEADER) {
                s_rx.frame[0] = data;
                s_rx.index = 1U;
                s_rx.expected_len = 0U;
                s_rx.state = VOFA_PARSE_WAIT_FF2;
            }
            break;

        case VOFA_PARSE_WAIT_FF2:
            if (data == FEETECH_STS_FRAME_HEADER) {
                s_rx.frame[1] = data;
                s_rx.index = 2U;
                s_rx.state = VOFA_PARSE_READ_FRAME;
            } else {
                VOFA_ResetParser();
                if (data == FEETECH_STS_FRAME_HEADER) {
                    s_rx.frame[0] = data;
                    s_rx.index = 1U;
                    s_rx.state = VOFA_PARSE_WAIT_FF2;
                }
            }
            break;

        case VOFA_PARSE_READ_FRAME:
            if (s_rx.index >= VOFA_FEETECH_FRAME_MAX_LEN) {
                s_vofa.rx_overflow_count++;
                VOFA_ResetParser();
                return;
            }

            s_rx.frame[s_rx.index++] = data;

            if (s_rx.index == 4U) {
                s_rx.expected_len = FeetechSts_GetFrameTotalLen(s_rx.frame, s_rx.index);
                if ((s_rx.expected_len < 6U) || (s_rx.expected_len > VOFA_FEETECH_FRAME_MAX_LEN)) {
                    s_vofa.frame_error_count++;
                    VOFA_ResetParser();
                }
                return;
            }

            if ((s_rx.expected_len != 0U) && (s_rx.index >= s_rx.expected_len)) {
                if ((FeetechSts_IsValidFrame(s_rx.frame, s_rx.expected_len) != 0U) ||
                    (ClawHostProtocol_IsFrame(s_rx.frame, s_rx.expected_len) != 0U)) {
                    VOFA_SaveFrame(s_rx.frame, s_rx.expected_len);
                } else {
                    s_vofa.frame_error_count++;
                }
                VOFA_ResetParser();
            }
            break;

        default:
            VOFA_ResetParser();
            break;
    }
}

// 保存完整帧到FIFO，避免按下帧和松开帧互相覆盖。
static void VOFA_SaveFrame(
    const uint8_t *frame,
    uint8_t len)
{
    uint8_t head;

    if ((frame == 0) ||
        (len == 0U) ||
        (len > VOFA_FEETECH_FRAME_MAX_LEN)) {
        return;
    }

    __disable_irq();

    /*
     * 队列满时丢弃最旧帧，保留最新松开帧。
     * 丢弃最新帧更容易造成按下保留、松开丢失。
     */
    if (s_vofa.frame_queue_count >=
        VOFA_RX_FRAME_QUEUE_DEPTH) {

        s_vofa.frame_tail++;
        if (s_vofa.frame_tail >=
            VOFA_RX_FRAME_QUEUE_DEPTH) {
            s_vofa.frame_tail = 0U;
        }

        s_vofa.frame_queue_count--;
        s_vofa.rx_overflow_count++;
    }

    head = s_vofa.frame_head;

    memcpy(
        (void *)s_vofa.frame_queue[head],
        frame,
        len);

    s_vofa.frame_len_queue[head] = len;

    s_vofa.frame_head++;
    if (s_vofa.frame_head >=
        VOFA_RX_FRAME_QUEUE_DEPTH) {
        s_vofa.frame_head = 0U;
    }

    s_vofa.frame_queue_count++;
    s_vofa.frame_count++;

    __enable_irq();
}

//启动 VOFA 单字节中断接收。
static HAL_StatusTypeDef VOFA_StartReceiveIT(void)
{
    /* 启动 VOFA UART 单字节接收。 */
    HAL_StatusTypeDef status;

    if (s_vofa.rx_active != 0U) {
        return HAL_OK;
    }

    switch(BoardUart_StartRxIT(BOARD_UART_HOST,(uint8_t *)&s_vofa.rx_byte)) {
     case BOARD_OK:status=HAL_OK;break;case BOARD_BUSY:status=HAL_BUSY;break;
     case BOARD_TIMEOUT:status=HAL_TIMEOUT;break;default:status=HAL_ERROR;break;
    }
    if (status == HAL_OK) {
        s_vofa.rx_active = 1U;
    }

    return status;
}

//写入 VOFA 发送环形缓冲。
static HAL_StatusTypeDef VOFA_WriteTxBuffer(const uint8_t *data, uint16_t len)
{
    /* 把待发送数据写入环形 TX 缓存。 */
    uint16_t i;
    uint16_t free_size;

    if ((data == 0) || (len == 0U)) {
        return HAL_OK;
    }

    if (len > VOFA_TX_BUFFER_SIZE) {
        s_vofa.tx_overflow_count++;
        return HAL_ERROR;
    }

    free_size = VOFA_TxFreeSizeUnsafe();
    if (len > free_size) {
        s_vofa.tx_overflow_count++;
        return HAL_BUSY;
    }

    for (i = 0U; i < len; i++) {
        s_vofa.tx_buf[s_tx.head] = data[i];
        s_tx.head++;
        if (s_tx.head >= VOFA_TX_BUFFER_SIZE) {
            s_tx.head = 0U;
        }
    }

    s_vofa.tx_count_in_buf = (uint16_t)(s_vofa.tx_count_in_buf + len);
    return HAL_OK;
}

//发送空闲且队列非空时启动发送。
static HAL_StatusTypeDef VOFA_TryStartTx(void)
{
    /* 空闲时启动下一段 DMA 发送。 */
    uint16_t i;
    uint16_t len;

    if (s_vofa.tx_busy != 0U) {
        return HAL_BUSY;
    }

    __disable_irq();
    if (s_vofa.tx_count_in_buf == 0U) {
        __enable_irq();
        return HAL_OK;
    }

    len = s_vofa.tx_count_in_buf;
    if (len > VOFA_TX_CHUNK_MAX_LEN) {
        len = VOFA_TX_CHUNK_MAX_LEN;
    }

    for (i = 0U; i < len; i++) {
        s_tx.active_buf[i] = s_vofa.tx_buf[s_tx.tail];
        s_tx.tail++;
        if (s_tx.tail >= VOFA_TX_BUFFER_SIZE) {
            s_tx.tail = 0U;
        }
    }

    s_tx.active_len = len;
    s_vofa.tx_busy = 1U;
    __enable_irq();

    if (BoardUart_TransmitIT(BOARD_UART_HOST,s_tx.active_buf,len) != BOARD_OK) {
        __disable_irq();
        s_vofa.tx_busy = 0U;
        s_tx.active_len = 0U;
        __enable_irq();
        return HAL_ERROR;
    }

    return HAL_OK;
}

//计算 VOFA 发送缓冲剩余空间。
static uint16_t VOFA_TxFreeSizeUnsafe(void)
{
    /* 计算 TX 环形缓存剩余空间。 */
    if (s_vofa.tx_count_in_buf >= VOFA_TX_BUFFER_SIZE) {
        return 0U;
    }
    return (uint16_t)(VOFA_TX_BUFFER_SIZE - s_vofa.tx_count_in_buf);
}

//刷新 VOFA 调试状态快照。
static void VOFA_UpdateStatus(void)
{
    /* 汇总 VOFA 运行状态快照。 */
    __disable_irq();
    s_status.rx_active = s_vofa.rx_active;
    s_status.last_byte = s_vofa.last_byte;
    s_status.frame_ready =(s_vofa.frame_queue_count != 0U) ? 1U : 0U;
    s_status.frame_queue_count = s_vofa.frame_queue_count;
    if (s_vofa.frame_queue_count != 0U) {
        s_status.frame_len = s_vofa.frame_len_queue[s_vofa.frame_tail];
    } else {
        s_status.frame_len = 0U;
    }
    s_status.tx_busy = s_vofa.tx_busy;
    s_status.tx_count_in_buf = s_vofa.tx_count_in_buf;
    s_status.last_rx_tick = s_vofa.last_rx_tick;
    s_status.rx_count = s_vofa.rx_count;
    s_status.tx_count = s_vofa.tx_count;
    s_status.frame_count = s_vofa.frame_count;
    s_status.frame_error_count = s_vofa.frame_error_count;
    s_status.rx_overflow_count = s_vofa.rx_overflow_count;
    s_status.tx_overflow_count = s_vofa.tx_overflow_count;
    s_status.uart_error_count = s_vofa.uart_error_count;
    __enable_irq();
}
