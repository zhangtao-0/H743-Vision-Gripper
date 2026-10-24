/*********************************************************************************************************************
 * @file    bsp_bluetooth.c
 * @brief   蓝牙 BSP 收发实现。
 *
 * @details
 * 1. 本文件负责：管理 USART1 中断收发、可选外层包和飞特帧缓存。
 * 2. 本文件不负责：解释舵机业务动作。
 * 3. 主要流程：UART 字节 -> 组帧 -> 缓存 -> 上层读取。
 *********************************************************************************************************************/

#include "bsp_bluetooth.h"
#include "claw_host_protocol.h"
#include <string.h>

typedef enum {
    BT_PARSE_WAIT_INPUT = 0,       // 空闲：FF -> 官方/F5 帧；A5 -> 外层包或滑块
    BT_PARSE_AFTER_APP_HEAD,       // A5 后：FF -> 兼容外层包；其他 -> 滑块通道
    BT_PARSE_FEETECH_FRAME,
    BT_PARSE_WAIT_APP_TAIL,
    BT_PARSE_SLIDER_VALUE,
    BT_PARSE_WAIT_SLIDER_TAIL
} BluetoothParseState_t;

typedef struct {
    volatile uint8_t initialized;

    volatile uint8_t rx_byte;
    volatile uint8_t rx_active;
    volatile uint8_t last_byte;
    volatile uint8_t frame_queue[BLUETOOTH_RX_FRAME_QUEUE_DEPTH][BLUETOOTH_FEETECH_FRAME_MAX_LEN];
    volatile uint8_t frame_len_queue[BLUETOOTH_RX_FRAME_QUEUE_DEPTH];
    volatile uint8_t frame_head;
    volatile uint8_t frame_tail;
    volatile uint8_t frame_queue_count;
    volatile uint8_t slider_channel;
    volatile uint8_t slider_percent;
    volatile uint8_t slider_ready;

    volatile uint8_t tx_buf[BLUETOOTH_TX_BUFFER_SIZE];
    volatile uint16_t tx_count_in_buf;
    volatile uint8_t tx_busy;

    volatile uint32_t rx_count;
    volatile uint32_t frame_count;
    volatile uint32_t slider_count;
    volatile uint32_t slider_error_count;
    volatile uint32_t tx_count;
    volatile uint32_t tx_overflow_count;
    volatile uint32_t overflow_count;
    volatile uint32_t frame_error_count;
    volatile uint32_t uart_error_count;
    volatile uint32_t last_rx_tick;
} BluetoothRuntime_t;
typedef struct {
    BluetoothParseState_t state;
    uint8_t frame[BLUETOOTH_FEETECH_FRAME_MAX_LEN];
    uint8_t index;
    uint8_t expected_len;
    uint8_t wrap_type;
    uint8_t slider_channel;
    uint8_t slider_payload[BLUETOOTH_SLIDER_PAYLOAD_MAX_LEN];
    uint8_t slider_index;
} BluetoothParser_t;

typedef struct {
    uint16_t head;
    uint16_t tail;
    uint8_t active_buf[BLUETOOTH_TX_CHUNK_MAX_LEN];
    uint16_t active_len;
} BluetoothTxRuntime_t;

static BluetoothRuntime_t s_bt;
static BluetoothParser_t s_parser;
static BluetoothTxRuntime_t s_tx;
static BluetoothStatus_t s_status;
//================================内部函数体===================================================
static void Bluetooth_StartReceiveIT(void);                         // 启动 单字节中断接收
static void Bluetooth_TryStartTransmitIT(void);                     // 发送队列非空且 UART 空闲 -> 启动下一块

static void Bluetooth_ProcessRxByte(uint8_t data);                  // 处理单字节接收：         外层包或裸飞特帧解析
static void Bluetooth_ProcessFeetechByte(uint8_t data);             // 组帧：FF FF -> Len -> CheckSum
static void Bluetooth_ProcessSliderByte(uint8_t data);              // 解析滑块整数和包尾
static uint8_t Bluetooth_DecodeSliderValue(const uint8_t *data, uint8_t len, uint8_t *value);
static void Bluetooth_StoreFeetechFrame(const uint8_t *frame, uint8_t len); // 缓存完整帧，置标志
static void Bluetooth_StoreSliderEvent(uint8_t channel, uint8_t percent);   // 缓存最新滑块事件

static void Bluetooth_ResetParser(void);                            // 复位当前未完成的解析。
static void Bluetooth_UpdateStatus(void);                           // 刷新对外状态快照。
static BSP_BT_Status_t Bluetooth_LoadTxBuffer(const uint8_t *data, uint16_t len); // 数据进入发送环形队列。
static BSP_BT_Status_t Bluetooth_WriteTxBufferUnsafe(const uint8_t *data, uint16_t len);
static uint16_t Bluetooth_TxFreeSizeUnsafe(void);

// 初始化蓝牙串口状态和接收中断。
void Bluetooth_Init(void)
{
    __disable_irq();
    memset((void *)&s_bt, 0, sizeof(s_bt));
    memset(&s_parser, 0, sizeof(s_parser));
    memset(&s_tx, 0, sizeof(s_tx));
    memset(&s_status, 0, sizeof(s_status));
    s_bt.initialized = 1U;
    Bluetooth_ResetParser();
    __enable_irq();

    Bluetooth_StartReceiveIT();
}

// 主循环维护蓝牙收发，不执行上层动作。
void Bluetooth_Task(void)
{
    uint32_t now;

    if (s_bt.initialized == 0U) {
        return;
    }
    if (s_bt.rx_active == 0U) {
        Bluetooth_StartReceiveIT();
    }

    now = HAL_GetTick();
    if ((s_parser.state != BT_PARSE_WAIT_INPUT) &&
        ((uint32_t)(now - s_bt.last_rx_tick) > BLUETOOTH_RX_PACKET_TIMEOUT_MS)) {
        __disable_irq();
        if ((s_parser.state != BT_PARSE_WAIT_INPUT) &&
            ((uint32_t)(now - s_bt.last_rx_tick) > BLUETOOTH_RX_PACKET_TIMEOUT_MS)) {
            Bluetooth_ResetParser();
            s_bt.frame_error_count++;
        }
        __enable_irq();
    }
    Bluetooth_TryStartTransmitIT();
}

// 兼容旧定时入口。
void Bluetooth_TimerIRQHandler(void)
{
    Bluetooth_Task();
}

//接收单字节 并持续
void Bluetooth_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((huart == 0) || (!BoardUart_IsInstance(huart,BOARD_UART_BLUETOOTH))) {
        return;
    }

    s_bt.rx_active = 0U;
    s_bt.last_byte = s_bt.rx_byte;
    s_bt.rx_count++;
    s_bt.last_rx_tick = HAL_GetTick();
    Bluetooth_ProcessRxByte(s_bt.rx_byte);      //单字节接收 处理
    Bluetooth_StartReceiveIT();
}

// 当前块发送完成 -> 从环形队列移除 -> 立即发送下一块。
void Bluetooth_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uint16_t sent_len;

    if ((huart == 0) || (!BoardUart_IsInstance(huart,BOARD_UART_BLUETOOTH))) {
        return;
    }

    __disable_irq();
    sent_len = s_tx.active_len;
    if (sent_len > s_bt.tx_count_in_buf) {
        sent_len = s_bt.tx_count_in_buf;
    }
    s_tx.tail = (uint16_t)((s_tx.tail + sent_len) % BLUETOOTH_TX_BUFFER_SIZE);
    s_bt.tx_count_in_buf = (uint16_t)(s_bt.tx_count_in_buf - sent_len);
    s_bt.tx_count += sent_len;
    s_tx.active_len = 0U;
    s_bt.tx_busy = 0U;
    __enable_irq();

    Bluetooth_TryStartTransmitIT();
}

//记录错误次数并重启
void Bluetooth_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart == 0) || (!BoardUart_IsInstance(huart,BOARD_UART_BLUETOOTH))) {
        return;
    }

    s_bt.uart_error_count++;
    s_bt.rx_active = 0U;
    Bluetooth_ResetParser();
}

#if (BLUETOOTH_ENABLE_HAL_UART_CALLBACK != 0U)
//HAL 接收回调入口，按串口实例分发。
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    Bluetooth_RxCpltCallback(huart);
}

//HAL 发送回调入口，按串口实例分发。
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    Bluetooth_TxCpltCallback(huart);
}

//HAL 错误回调入口，按串口实例分发。
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    Bluetooth_ErrorCallback(huart);
}
#endif

// 读取队首完整帧：保持按下/松开等命令的到达顺序。
uint8_t Bluetooth_ReadFeetechFrame(uint8_t *buf, uint8_t buf_size, uint8_t *len)
{
    uint8_t copy_len;
    uint8_t tail;

    if ((buf == 0) || (len == 0) || (buf_size == 0U)) {
        return 0U;
    }

    __disable_irq();
    if (s_bt.frame_queue_count == 0U) {
        *len = 0U;
        __enable_irq();
        return 0U;
    }
    tail = s_bt.frame_tail;
    copy_len = s_bt.frame_len_queue[tail];
    if (copy_len > buf_size) {
        copy_len = buf_size;
    }
    memcpy(buf, (const void *)s_bt.frame_queue[tail], copy_len);
    *len = copy_len;
    s_bt.frame_tail++;
    if (s_bt.frame_tail >= BLUETOOTH_RX_FRAME_QUEUE_DEPTH) {
        s_bt.frame_tail = 0U;
    }
    s_bt.frame_queue_count--;
    __enable_irq();
    return 1U;
}

// 清除全部待处理完整帧。
void Bluetooth_ClearFeetechFrame(void)
{
    __disable_irq();
    s_bt.frame_head = 0U;
    s_bt.frame_tail = 0U;
    s_bt.frame_queue_count = 0U;
    __enable_irq();
}

// 读取滑块事件：滑动期间只取最新值。
uint8_t Bluetooth_ReadSliderEvent(BluetoothSliderEvent_t *event)
{
    if (event == 0) {
        return 0U;
    }

    __disable_irq();
    if (s_bt.slider_ready == 0U) {
        __enable_irq();
        return 0U;
    }
    event->channel = s_bt.slider_channel;
    event->percent = s_bt.slider_percent;
    s_bt.slider_ready = 0U;
    __enable_irq();
    return 1U;
}
// 发送完整飞特帧：按蓝牙传输模式选择裸帧或固定外层包。
BSP_BT_Status_t Bluetooth_SendFeetechFrame(const uint8_t *frame, uint8_t len)
{
#if (BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_RAW)
    uint8_t tx_frame[BLUETOOTH_FEETECH_FRAME_MAX_LEN + 2U];
#endif

    if ((frame == 0) || (len == 0U)) {
        return BSP_BT_BAD_PARAM;
    }

    if ((FeetechSts_IsValidFrame(frame, len) == 0U) &&
        (ClawHostProtocol_IsFrame(frame, len) == 0U)) {
        s_bt.frame_error_count++;
        return BSP_BT_BAD_FRAME;
    }

#if (BLUETOOTH_TRANSPORT_MODE == BLUETOOTH_TRANSPORT_RAW)
    return Bluetooth_LoadTxBuffer(frame, len);
#else
    if ((uint16_t)len + 2U > sizeof(tx_frame)) {
        return BSP_BT_OVERFLOW;
    }

    tx_frame[0] = BLUETOOTH_APP_HEAD;
    memcpy(&tx_frame[1], frame, len);
    tx_frame[(uint8_t)(len + 1U)] = BLUETOOTH_APP_TAIL;
    return Bluetooth_LoadTxBuffer(tx_frame, (uint16_t)(len + 2U));
#endif
}

// 组飞特命令并按当前蓝牙传输模式发送。
BSP_BT_Status_t Bluetooth_SendFeetechCommand(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len)
{
    uint8_t frame[BLUETOOTH_FEETECH_FRAME_MAX_LEN];
    uint8_t len;

    len = FeetechSts_BuildPacket(id, instruction, params, param_len, frame, sizeof(frame));
    if (len == 0U) {
        return BSP_BT_BAD_PARAM;
    }

    return Bluetooth_SendFeetechFrame(frame, len);
}

// 对外状态快照。
const BluetoothStatus_t *Bluetooth_GetStatus(void)
{
    Bluetooth_UpdateStatus();
    return &s_status;
}

//读取缓存的文本包快照。
uint8_t Bluetooth_ReadPacket(uint8_t *buf, uint8_t buf_size, uint8_t *len)
{
    return Bluetooth_ReadFeetechFrame(buf, buf_size, len);
}

//清除文本包 ready 标志。
void Bluetooth_ClearPacket(void)
{
    Bluetooth_ClearFeetechFrame();
}

//发送文本包到蓝牙。
BSP_BT_Status_t Bluetooth_SendPacket(const uint8_t *frame, uint8_t len)
{
    return Bluetooth_SendFeetechFrame(frame, len);
}

// 原始字节直接写入蓝牙发送队列。
BSP_BT_Status_t Bluetooth_SendRaw(const uint8_t *data, uint16_t len)
{
    return Bluetooth_LoadTxBuffer(data, len);
}

// 文本和换行作为一个事务写入环形队列。
BSP_BT_Status_t Bluetooth_SendTextLine(const char *text)
{
    uint16_t len;
    BSP_BT_Status_t status;
    static const uint8_t crlf[2] = { '\r', '\n' };

    if (text == 0) {
        return BSP_BT_BAD_PARAM;
    }
    len = (uint16_t)strlen(text);
    if ((len == 0U) || ((uint32_t)len + 2UL > BLUETOOTH_TX_BUFFER_SIZE)) {
        return BSP_BT_BAD_PARAM;
    }

    __disable_irq();
    if ((uint16_t)(len + 2U) > Bluetooth_TxFreeSizeUnsafe()) {
        s_bt.tx_overflow_count++;
        status = BSP_BT_BUSY;
    } else {
        status = Bluetooth_WriteTxBufferUnsafe((const uint8_t *)text, len);
        if (status == BSP_BT_OK) {
            status = Bluetooth_WriteTxBufferUnsafe(crlf, 2U);
        }
    }
    __enable_irq();

    if (status == BSP_BT_OK) {
        Bluetooth_TryStartTransmitIT();
    }
    return status;
}

//使能单字节中断接收
static void Bluetooth_StartReceiveIT(void)
{
    BoardStatus status;

    if (s_bt.rx_active != 0U) {
        return;
    }

    status = BoardUart_StartRxIT(BOARD_UART_BLUETOOTH,(uint8_t *)&s_bt.rx_byte);
    if ((status == BOARD_OK) || (status == BOARD_BUSY)) {
        s_bt.rx_active = 1U;
    } else {
        s_bt.uart_error_count++;
        s_bt.rx_active = 0U;
    }
}

// 发送空闲且队列非空时，复制队首数据并启动中断发送。
static void Bluetooth_TryStartTransmitIT(void)
{
    BoardStatus status;
    uint16_t i;
    uint16_t index;
    uint16_t len;

    __disable_irq();
    if ((s_bt.tx_busy != 0U) || (s_bt.tx_count_in_buf == 0U)) {
        __enable_irq();
        return;
    }
    len = s_bt.tx_count_in_buf;
    if (len > BLUETOOTH_TX_CHUNK_MAX_LEN) {
        len = BLUETOOTH_TX_CHUNK_MAX_LEN;
    }
    index = s_tx.tail;
    for (i = 0U; i < len; i++) {
        s_tx.active_buf[i] = s_bt.tx_buf[index];
        index++;
        if (index >= BLUETOOTH_TX_BUFFER_SIZE) {
            index = 0U;
        }
    }
    s_tx.active_len = len;
    s_bt.tx_busy = 1U;
    __enable_irq();

    status = BoardUart_TransmitIT(BOARD_UART_BLUETOOTH,s_tx.active_buf,len);
    if (status != BOARD_OK) {
        __disable_irq();
        s_bt.tx_busy = 0U;
        s_tx.active_len = 0U;
        if (status != BOARD_BUSY) {
            s_bt.uart_error_count++;
        }
        __enable_irq();
    }
}

// 原始字节写入发送环形队列，并立即尝试发送。
static BSP_BT_Status_t Bluetooth_LoadTxBuffer(const uint8_t *data, uint16_t len)
{
    BSP_BT_Status_t status;

    if ((data == 0) || (len == 0U)) {
        return BSP_BT_BAD_PARAM;
    }

    __disable_irq();
    status = Bluetooth_WriteTxBufferUnsafe(data, len);
    __enable_irq();
    if (status == BSP_BT_OK) {
        Bluetooth_TryStartTransmitIT();
    }
    return status;
}

// 已在临界区内：数据追加到 TX 环形队列。
static BSP_BT_Status_t Bluetooth_WriteTxBufferUnsafe(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0U) || (len > BLUETOOTH_TX_BUFFER_SIZE)) {
        return BSP_BT_BAD_PARAM;
    }
    if (len > Bluetooth_TxFreeSizeUnsafe()) {
        s_bt.tx_overflow_count++;
        return BSP_BT_BUSY;
    }
    for (i = 0U; i < len; i++) {
        s_bt.tx_buf[s_tx.head] = data[i];
        s_tx.head++;
        if (s_tx.head >= BLUETOOTH_TX_BUFFER_SIZE) {
            s_tx.head = 0U;
        }
    }
    s_bt.tx_count_in_buf = (uint16_t)(s_bt.tx_count_in_buf + len);
    return BSP_BT_OK;
}

// 计算 TX 环形队列剩余空间。
static uint16_t Bluetooth_TxFreeSizeUnsafe(void)
{
    if (s_bt.tx_count_in_buf >= BLUETOOTH_TX_BUFFER_SIZE) {
        return 0U;
    }
    return (uint16_t)(BLUETOOTH_TX_BUFFER_SIZE - s_bt.tx_count_in_buf);
}
// 单字节入口：裸官方/F5 帧与滑块包分别解析。
static void Bluetooth_ProcessRxByte(uint8_t data)
{
    switch (s_parser.state) {
        case BT_PARSE_WAIT_INPUT:
            if (data == BLUETOOTH_SLIDER_HEAD) {
                s_parser.state = BT_PARSE_AFTER_APP_HEAD;
                return;
            }
#if (BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_WRAPPED)
            if (data == FEETECH_STS_FRAME_HEADER) {
                s_parser.wrap_type = 0U;
                s_parser.state = BT_PARSE_FEETECH_FRAME;
                Bluetooth_ProcessFeetechByte(data);
            }
#endif
            return;

        case BT_PARSE_AFTER_APP_HEAD:
#if (BLUETOOTH_TRANSPORT_MODE != BLUETOOTH_TRANSPORT_RAW)
            if (data == FEETECH_STS_FRAME_HEADER) {
                s_parser.wrap_type = 1U;
                s_parser.state = BT_PARSE_FEETECH_FRAME;
                Bluetooth_ProcessFeetechByte(data);
                return;
            }
#endif
#if (BLUETOOTH_SLIDER_ENABLE != 0U)
            s_parser.slider_channel = data;
            s_parser.slider_index = 0U;
            s_parser.state = BT_PARSE_SLIDER_VALUE;
#else
            s_bt.slider_error_count++;
            Bluetooth_ResetParser();
#endif
            return;

        case BT_PARSE_FEETECH_FRAME:
            Bluetooth_ProcessFeetechByte(data);
            return;

        case BT_PARSE_WAIT_APP_TAIL:
            if (data == BLUETOOTH_APP_TAIL) {
                Bluetooth_StoreFeetechFrame(s_parser.frame, s_parser.expected_len);
            } else {
                s_bt.frame_error_count++;
            }
            Bluetooth_ResetParser();
            return;

        case BT_PARSE_SLIDER_VALUE:
        case BT_PARSE_WAIT_SLIDER_TAIL:
            Bluetooth_ProcessSliderByte(data);
            return;

        default:
            s_bt.frame_error_count++;
            Bluetooth_ResetParser();
            return;
    }
}
// 处理飞特帧字节：FF FF -> Len -> 收满 -> 校验 -> 缓存/等待 5A。
static void Bluetooth_ProcessFeetechByte(uint8_t data)
{
    if (s_parser.index == 0U) {                                      // 等第 1 个 FF
        if (data != FEETECH_STS_FRAME_HEADER) {
            s_bt.frame_error_count++;                                // 帧头错误 -> 计错
            Bluetooth_ResetParser();
            return;
        }
    } else if (s_parser.index == 1U) {                               // 等第 2 个 FF
        if (data != FEETECH_STS_FRAME_HEADER) {
            s_bt.frame_error_count++;                                // 第二个帧头错误 -> 计错
            Bluetooth_ResetParser();

            if (data == FEETECH_STS_FRAME_HEADER) {                  // 当前字节可能是新帧第 1 个 FF
                s_parser.state = BT_PARSE_FEETECH_FRAME;
                s_parser.frame[s_parser.index++] = data;
            }
            return;
        }
    }

    if (s_parser.index >= BLUETOOTH_FEETECH_FRAME_MAX_LEN) {         // 组帧缓存满 -> 溢出
        s_bt.overflow_count++;
        Bluetooth_ResetParser();
        return;
    }

    s_parser.frame[s_parser.index++] = data;                         // 当前字节 -> 飞特帧缓存

    if (s_parser.index == 4U) {                                      // 收到 Len -> 计算整帧长度
        s_parser.expected_len = FeetechSts_GetFrameTotalLen(s_parser.frame, s_parser.index);
        if (s_parser.expected_len == 0U) {
            s_bt.frame_error_count++;                                // Len 不合法 -> 计错
            Bluetooth_ResetParser();
            return;
        }
    }

    if ((s_parser.expected_len != 0U) && (s_parser.index >= s_parser.expected_len)) {
        if ((FeetechSts_IsValidFrame(s_parser.frame, s_parser.expected_len) == 0U) &&
            (ClawHostProtocol_IsFrame(s_parser.frame, s_parser.expected_len) == 0U)) {
            s_bt.frame_error_count++;                                // 校验失败 -> 丢弃
            Bluetooth_ResetParser();
            return;
        }

        if (s_parser.wrap_type != 0U) {
            s_parser.state = BT_PARSE_WAIT_APP_TAIL;                 // A5 外层包 -> 继续等 5A
            return;
        }

        Bluetooth_StoreFeetechFrame(s_parser.frame, s_parser.expected_len); // 裸飞特帧 -> 直接缓存
        Bluetooth_ResetParser();
        return;
    }
}

// 解析滑块值；ASCII 用包尾定界，二进制格式按固定字节数接收。
static void Bluetooth_ProcessSliderByte(uint8_t data)
{
    uint8_t expected_len;
    uint8_t value;

#if (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_ASCII)
    if (data == BLUETOOTH_SLIDER_TAIL) {
        if (Bluetooth_DecodeSliderValue(s_parser.slider_payload,
                                        s_parser.slider_index,
                                        &value) != 0U) {
            Bluetooth_StoreSliderEvent(s_parser.slider_channel, value);
        } else {
            s_bt.slider_error_count++;
        }
        Bluetooth_ResetParser();
        return;
    }
#else
    if (s_parser.state == BT_PARSE_WAIT_SLIDER_TAIL) {
        if ((data == BLUETOOTH_SLIDER_TAIL) &&
            (Bluetooth_DecodeSliderValue(s_parser.slider_payload,
                                         s_parser.slider_index,
                                         &value) != 0U)) {
            Bluetooth_StoreSliderEvent(s_parser.slider_channel, value);
        } else {
            s_bt.slider_error_count++;
        }
        Bluetooth_ResetParser();
        return;
    }
#endif

    if (s_parser.slider_index >= BLUETOOTH_SLIDER_PAYLOAD_MAX_LEN) {
        s_bt.slider_error_count++;
        Bluetooth_ResetParser();
        return;
    }
    s_parser.slider_payload[s_parser.slider_index++] = data;

#if (BLUETOOTH_SLIDER_VALUE_FORMAT != BLUETOOTH_SLIDER_VALUE_ASCII)
#if (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_U8)
    expected_len = 1U;
#elif (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_U16_LE)
    expected_len = 2U;
#else
    expected_len = 4U;
#endif
    if (s_parser.slider_index >= expected_len) {
        s_parser.state = BT_PARSE_WAIT_SLIDER_TAIL;
    }
#else
    expected_len = 0U;
    (void)expected_len;
#endif
}

// 将配置的数据形式统一转换为 0~100。
static uint8_t Bluetooth_DecodeSliderValue(const uint8_t *data, uint8_t len, uint8_t *value)
{
    uint32_t result;
    uint8_t i;

    if ((data == 0) || (value == 0) || (len == 0U)) {
        return 0U;
    }
    result = 0UL;

#if (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_ASCII)
    if (len > 3U) {
        return 0U;
    }
    for (i = 0U; i < len; i++) {
        if ((data[i] < (uint8_t)'0') || (data[i] > (uint8_t)'9')) {
            return 0U;
        }
        result = result * 10UL + (uint32_t)(data[i] - (uint8_t)'0');
    }
#elif (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_U8)
    if (len != 1U) {
        return 0U;
    }
    result = data[0];
#elif (BLUETOOTH_SLIDER_VALUE_FORMAT == BLUETOOTH_SLIDER_VALUE_U16_LE)
    if (len != 2U) {
        return 0U;
    }
    result = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
#else
    if (len != 4U) {
        return 0U;
    }
    result = (uint32_t)data[0] |
             ((uint32_t)data[1] << 8) |
             ((uint32_t)data[2] << 16) |
             ((uint32_t)data[3] << 24);
#endif

    if (result > BLUETOOTH_SLIDER_VALUE_MAX) {
        return 0U;
    }
    *value = (uint8_t)result;
    return 1U;
}
// 完整官方/F5 帧进入 FIFO；队满时保留已有顺序并拒绝新帧。
static void Bluetooth_StoreFeetechFrame(const uint8_t *frame, uint8_t len)
{
    uint8_t head;

    if ((frame == 0) || (len == 0U) || (len > BLUETOOTH_FEETECH_FRAME_MAX_LEN)) {
        s_bt.frame_error_count++;
        return;
    }

    __disable_irq();
    if (s_bt.frame_queue_count >= BLUETOOTH_RX_FRAME_QUEUE_DEPTH) {
        s_bt.overflow_count++;
        __enable_irq();
        return;
    }
    head = s_bt.frame_head;
    memcpy((void *)s_bt.frame_queue[head], frame, len);
    s_bt.frame_len_queue[head] = len;
    s_bt.frame_head++;
    if (s_bt.frame_head >= BLUETOOTH_RX_FRAME_QUEUE_DEPTH) {
        s_bt.frame_head = 0U;
    }
    s_bt.frame_queue_count++;
    s_bt.frame_count++;
    __enable_irq();
}
// 缓存最新滑块事件；连续拖动时新值覆盖尚未处理的旧值。
static void Bluetooth_StoreSliderEvent(uint8_t channel, uint8_t percent)
{
    __disable_irq();
    if (s_bt.slider_ready != 0U) {
        s_bt.overflow_count++;
    }
    s_bt.slider_channel = channel;
    s_bt.slider_percent = percent;
    s_bt.slider_ready = 1U;
    s_bt.slider_count++;
    __enable_irq();
}

// 复位解析器：丢弃未完成帧或滑块包。
static void Bluetooth_ResetParser(void)
{
    s_parser.state = BT_PARSE_WAIT_INPUT;
    s_parser.index = 0U;
    s_parser.expected_len = 0U;
    s_parser.wrap_type = 0U;
    s_parser.slider_channel = 0U;
    s_parser.slider_index = 0U;
    memset(s_parser.frame, 0, sizeof(s_parser.frame));
    memset(s_parser.slider_payload, 0, sizeof(s_parser.slider_payload));
}

//刷新蓝牙调试状态快照：内部运行状态 s_bt -> 对外只读状态 s_status。
static void Bluetooth_UpdateStatus(void)
{
    __disable_irq();
    s_status.initialized = s_bt.initialized;
    s_status.rx_active = s_bt.rx_active;
    s_status.last_byte = s_bt.last_byte;
    s_status.frame_ready = (s_bt.frame_queue_count != 0U) ? 1U : 0U;
    s_status.frame_len = (s_bt.frame_queue_count != 0U) ?
                         s_bt.frame_len_queue[s_bt.frame_tail] : 0U;
    s_status.frame_queue_count = s_bt.frame_queue_count;
    s_status.tx_pending = (s_bt.tx_count_in_buf != 0U) ? 1U : 0U;
    s_status.tx_busy = s_bt.tx_busy;
    s_status.tx_len = s_bt.tx_count_in_buf;
    s_status.rx_count = s_bt.rx_count;
    s_status.frame_count = s_bt.frame_count;
    s_status.slider_ready = s_bt.slider_ready;
    s_status.slider_channel = s_bt.slider_channel;
    s_status.slider_percent = s_bt.slider_percent;
    s_status.slider_count = s_bt.slider_count;
    s_status.slider_error_count = s_bt.slider_error_count;
    s_status.tx_count = s_bt.tx_count;
    s_status.tx_overflow_count = s_bt.tx_overflow_count;
    s_status.overflow_count = s_bt.overflow_count;
    s_status.frame_error_count = s_bt.frame_error_count;
    s_status.uart_error_count = s_bt.uart_error_count;
    s_status.last_rx_tick = s_bt.last_rx_tick;
    __enable_irq();
}
