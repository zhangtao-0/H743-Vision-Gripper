/*********************************************************************************************************************
 * @file    feetech_sts_bus.c
 * @brief   飞特 STS 总线协议实现。
 *
 * @details
 * 1. 本文件负责：STS 帧构造、发送、接收解析和反馈缓存。
 * 2. 本文件不负责：处理 A0/B0 业务动作。
 * 3. 主要流程：上层命令 -> UART 帧 -> 状态包 -> feedback。
 *********************************************************************************************************************/

#include "feetech_sts_bus.h"
#include "feetech_sts_port.h"
#include <string.h>

/*============================================================
 * 1. 内部运行结构
 *============================================================*/

typedef struct {
    FeetechStsBus_t bus;                              // 对外总线状态：发送状态/应答包/反馈缓存/运动目标

    // 接收组帧状态：UART 单字节 -> rx_frame -> 完整飞特应答帧。
    uint8_t rx_byte;                                  // UART 单字节接收缓存
    uint8_t rx_frame[FEETECH_STS_FRAME_MAX_LEN];      // 正在组包的接收帧缓存
    uint8_t rx_index;                                 // 当前已接收字节数
    uint8_t rx_expected_len;                          // 当前帧期望总长度

    // 等待应答状态：记录刚发送的命令 -> 用于解析返回包。
    FeetechStsCommand_t pending_command;              // 等待应答的命令类型
    uint8_t pending_read_addr;                        // READ 命令读取起始地址
    uint8_t pending_read_len;                         // READ 命令读取长度

    // 运动目标发送记录：避免重复发送 -> 周期保持发送。
    uint8_t motion_sent_valid;                        // 当前运动目标是否已发送过
    float last_sent_angle_deg;                        // 最近已发送目标角度
    uint16_t last_sent_time_ms;                       // 最近已发送目标时间
    uint16_t last_sent_speed;                         // 最近已发送目标速度
    uint8_t last_sent_acc;                            // 最近已发送目标加速度
    uint32_t last_motion_send_tick;                   // 最近一次运动目标发送时间戳
    uint32_t tx_echo_drop_count;                      //丢弃发送回显次数。
} FeetechStsRuntime_t;  

static volatile uint32_t s_raw_rx_bytes;
uint32_t FeetechStsBus_RawRxBytes(void) { return s_raw_rx_bytes; }

static FeetechStsRuntime_t s_feetech_sts;             // 飞特总线内部运行状态：接收组帧/等待应答/运动发送记录

/*============================================================
 * 2. 内部函数声明
 *============================================================*/

static HAL_StatusTypeDef FeetechSts_SendPacket(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len, FeetechStsCommand_t command); // 参数 -> 完整飞特帧 -> UART 发送。
static FeetechStsCommand_t FeetechSts_MapInstruction(uint8_t instruction);       // 飞特指令码 -> 内部命令类型。
static uint8_t FeetechSts_FrameExpectResponse(const uint8_t *frame, uint8_t len); // 发送帧 -> 判断是否需要等待应答。

uint8_t FeetechSts_BuildPacket(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len, uint8_t *out, uint8_t out_max); // ID/指令/参数 -> 完整飞特帧。
uint8_t FeetechSts_CalcChecksum(const uint8_t *packet, uint8_t total_len);       // 飞特帧内容 -> CheckSum。
uint8_t FeetechSts_VerifyChecksum(const uint8_t *packet, uint8_t total_len);     // 飞特帧 -> 校验结果。
static void FeetechSts_ResetRxParser(void);                                      // 复位接收解析器：丢弃未完成帧 -> 回到等待帧头。
static void FeetechSts_ProcessRxByte(uint8_t data);                              // 接收字节 -> 组飞特状态包 -> 收满后解析。
static void FeetechSts_ParseStatusPacket(const uint8_t *packet, uint8_t total_len); // 状态包 -> 分类 ACK/READ -> 更新 packet/feedback。
static void FeetechSts_ApplyReadData(uint8_t address, const uint8_t *params, uint8_t len); // READ 数据 -> 按地址解析 -> 更新反馈缓存。
static uint8_t FeetechSts_FieldAvailable(uint8_t base, uint8_t len, uint8_t field, uint8_t field_len, uint8_t *offset); // 读取范围 -> 判断字段是否完整包含。
static int16_t FeetechSts_MakeS16(uint8_t low, uint8_t high);                    // 低字节/高字节 -> int16_t。
static uint16_t FeetechSts_LimitTime(uint16_t time_ms);                          // 输入时间 -> 限制到合法时间范围。
static uint16_t FeetechSts_LimitSpeed(uint16_t speed);                           // 输入速度 -> 限制到合法速度范围。
static void FeetechSts_UpdateReached(void);                                      // 当前角度/目标角度 -> 更新到位标志。
static float FeetechSts_AbsFloatDiff(float a, float b);                          // 两个浮点数 -> 绝对差值。
static void FeetechSts_RecordTx(FeetechStsCommand_t command, uint8_t id, uint8_t instruction, uint8_t address, uint8_t data_len, const uint8_t *frame, uint8_t frame_len); // 发送信息 -> 记录最近发送状态。
/*============================================================
 * 3. 初始化、接收中断和状态读取
 *============================================================*/

/// 初始化 STS 总线：清运行状态 -> 设置默认运动目标 -> 启动 UART 接收。
void FeetechStsBus_Init(void)
{
    memset(&s_feetech_sts, 0, sizeof(s_feetech_sts));    // 清总线运行状态/反馈缓存/接收解析器

    s_feetech_sts.bus.last_status = HAL_OK;
    s_feetech_sts.bus.motion.id = FEETECH_STS_DEFAULT_ID;
    s_feetech_sts.bus.motion.target_angle_deg = FEETECH_STS_ANGLE_MIN_DEG;
    s_feetech_sts.bus.motion.target_position = FeetechSts_AngleToPosition(FEETECH_STS_ANGLE_MIN_DEG);
    s_feetech_sts.bus.motion.target_time_ms = FEETECH_STS_DEFAULT_TIME_MS;
    s_feetech_sts.bus.motion.target_speed = FEETECH_STS_DEFAULT_SPEED;
    s_feetech_sts.bus.motion.target_acc = FEETECH_STS_DEFAULT_ACC;

    s_feetech_sts.last_sent_angle_deg = FEETECH_STS_ANGLE_MIN_DEG;
    s_feetech_sts.last_sent_time_ms = FEETECH_STS_DEFAULT_TIME_MS;
    s_feetech_sts.last_sent_speed = FEETECH_STS_DEFAULT_SPEED;
    s_feetech_sts.last_sent_acc = FEETECH_STS_DEFAULT_ACC;

    FeetechSts_ResetRxParser();                         // 复位接收解析器
    (void)FeetechStsBus_StartReceive();                 // 启动 UART 接收
}

// 启动总线接收：UART 空闲检查 -> 清溢出标志 -> 挂起单字节中断接收。
HAL_StatusTypeDef FeetechStsBus_StartReceive(void)
{
    HAL_StatusTypeDef status;                            // HAL 接收启动结果


    status = FeetechStsPort_StartReceiveIT(
        &s_feetech_sts.rx_byte);

    /*
     * 接收已挂起时返回 HAL_BUSY 是正常情况，
     * 不要因此覆盖刚刚成功的发送状态。
     */
    if (status != HAL_BUSY) {
        s_feetech_sts.bus.last_status = status;
    }

    return status;
}

// 接收完成 -> 喂入状态包解析器 -> 继续接收下一字节。
void FeetechStsBus_HandleRxComplete(void)
{
    s_raw_rx_bytes++;
    FeetechSts_ProcessRxByte(s_feetech_sts.rx_byte);     // 当前字节 -> 状态包解析器
    (void)FeetechStsBus_StartReceive();                  // 继续挂起下一字节接收
}

// 取走反馈更新标志：读取 update_flags -> 清零。
uint8_t FeetechStsBus_TakeUpdateFlags(void)
{
    uint8_t flags;                                       // 本次取出的更新标志

    __disable_irq();                                     // 保护 update_flags
    flags = s_feetech_sts.bus.feedback.update_flags;     // 读取更新标志
    s_feetech_sts.bus.feedback.update_flags = 0U;        // 取走后清零
    __enable_irq();

    return flags;
}

const FeetechStsBus_t *FeetechStsBus_GetBus(void)
{
    return &s_feetech_sts.bus;
}

const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void)
{
    return &s_feetech_sts.bus.feedback;
}

const FeetechStsMotion_t *FeetechStsBus_GetMotion(void)
{
    return &s_feetech_sts.bus.motion;
}

// 获取最近 HAL 状态。
HAL_StatusTypeDef FeetechStsBus_GetLastStatus(void)
{
    return s_feetech_sts.bus.last_status;
}

// 取走最近原始应答包：packet.raw -> 外部 buffer -> 清 unread。
uint8_t FeetechStsBus_TakePacket(uint8_t *buffer, uint8_t buffer_size, uint8_t *out_len)
{
    FeetechStsPacket_t *packet;
    uint8_t i;

    if (out_len != 0) {
        *out_len = 0U;
    }
    if ((buffer == 0) || (out_len == 0)) {                         // 参数无效 -> 取包失败
        return 0U;
    }

    packet = &s_feetech_sts.bus.packet;                            // 最近一次有效应答包
    if ((packet->unread == 0U) ||                                   // 没有未读包
        (packet->raw_len == 0U) ||                                  // 原始帧长度无效
        (packet->raw_len > buffer_size)) {                          // 外部缓存不足
        return 0U;
    }

    for (i = 0U; i < packet->raw_len; i++) {                        // packet.raw -> 外部 buffer
        buffer[i] = packet->raw[i];
    }

    *out_len = packet->raw_len;                                     // 返回实际帧长度
    packet->unread = 0U;                                            // 本帧已取走 -> 清未读标志
    return 1U;
}

// 发送完整 STS 帧：校验帧 -> 记录上下文 -> UART2 发送 -> 重启接收。
HAL_StatusTypeDef FeetechStsBus_SendFrame(const uint8_t *frame, uint8_t len)
{
    HAL_StatusTypeDef status;
    uint8_t id;
    uint8_t instruction;
    uint8_t address;
    uint8_t data_len;

    if (FeetechSts_IsValidFrame(frame, len) == 0U) {                // 帧头/长度/校验失败 -> 拒绝发送
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    id = frame[2];                                                  // 提取目标 ID
    instruction = frame[4];                                         // 提取飞特指令码
    address = 0U;
    data_len = 0U;

    if ((instruction == FEETECH_STS_INST_READ) && (len >= 8U)) {     // READ -> 记录读取地址/长度
        address = frame[5];
        data_len = frame[6];
        s_feetech_sts.pending_read_addr = address;
        s_feetech_sts.pending_read_len = data_len;
    } else if (((instruction == FEETECH_STS_INST_WRITE) ||
                (instruction == FEETECH_STS_INST_REG_WRITE)) &&
               (len >= 7U)) {                                      // WRITE/REG_WRITE -> 记录写入地址/长度
        address = frame[5];
        data_len = (uint8_t)(frame[3] - 3U);
        s_feetech_sts.pending_read_addr = 0U;
        s_feetech_sts.pending_read_len = 0U;
    } else {                                                        // 其他指令 -> 清 READ 上下文
        s_feetech_sts.pending_read_addr = 0U;
        s_feetech_sts.pending_read_len = 0U;
    }

    s_feetech_sts.bus.packet.unread = 0U;                           // 发送新帧前丢弃旧未读包
    FeetechSts_RecordTx(FeetechSts_MapInstruction(instruction),
                        id,
                        instruction,
                        address,
                        data_len,
                        frame,
                        len);                                      // 记录最近发送状态
    FeetechSts_ResetRxParser();                                     // 发送前复位接收解析器
    FeetechStsPort_ClearOverrun();

    status = FeetechStsPort_Transmit(
        frame,
        len,
        FEETECH_STS_TX_TIMEOUT_MS);
    s_feetech_sts.bus.last_status = status;                         // 记录 HAL 发送结果

    if (status == HAL_OK) {
        s_feetech_sts.bus.tx.tx_count++;                            // 发送成功计数
    }

    (void)FeetechStsBus_StartReceive();                             // 发送后继续接收应答

    return status;
}

// 发送并等待应答：发送 STS 帧 -> 轮询 packet -> 返回原始应答。
HAL_StatusTypeDef FeetechStsBus_SendFrameAndWaitResponse(const uint8_t *frame,
                                                         uint8_t len,
                                                         uint8_t *response,
                                                         uint8_t response_size,
                                                         uint8_t *response_len,
                                                         uint32_t timeout_ms)
{
    uint32_t start_tick;
    HAL_StatusTypeDef status;

    if (response_len != 0) {
        *response_len = 0U;
    }
    if ((response == 0) || (response_len == 0)) {                    // 参数无效 -> 返回错误
        return HAL_ERROR;
    }

    status = FeetechStsBus_SendFrame(frame, len);                    // 发送完整 STS 帧
    if (status != HAL_OK) {
        return status;
    }

    if (FeetechSts_FrameExpectResponse(frame, len) == 0U) {          // 无需等待应答 -> 发送成功即返回
        return HAL_OK;
    }

    if (timeout_ms == 0U) {                                          // 未指定超时 -> 使用默认超时
        timeout_ms = FEETECH_STS_RESP_TIMEOUT_MS;
    }

    start_tick = HAL_GetTick();                                      // 记录等待起点
    while ((uint32_t)(HAL_GetTick() - start_tick) <= timeout_ms) {   // 超时窗口内轮询应答包
        if (FeetechStsBus_TakePacket(response, response_size, response_len) != 0U) {
            return HAL_OK;
        }
    }

    return HAL_TIMEOUT;                                              // 超时未收到应答
}

/*============================================================
 * 4. 基础协议接口
 *============================================================*/

// PING 探测：清 READ 上下文 -> 发送 PING 指令。
HAL_StatusTypeDef FeetechStsBus_Ping(uint8_t id)
{
    s_feetech_sts.pending_read_addr = 0U;    // 清 READ 地址上下文
    s_feetech_sts.pending_read_len = 0U;     // 清 READ 长度上下文
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_PING, 0, 0U, FEETECH_STS_CMD_PING);
}

// READ 读取寄存器：地址/长度 -> READ 参数 -> 发送 READ 指令。
HAL_StatusTypeDef FeetechStsBus_Read(uint8_t id, uint8_t address, uint8_t read_len)
{
    uint8_t params[2];                       // READ 参数：起始地址 + 读取长度

    if ((read_len == 0U) || (read_len > FEETECH_STS_PARAM_MAX_LEN)) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    params[0] = address;
    params[1] = read_len;
    s_feetech_sts.pending_read_addr = address;   // 记录 READ 起始地址，供回包解析使用
    s_feetech_sts.pending_read_len = read_len;   // 记录 READ 长度，供回包解析使用
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_READ, params, sizeof(params), FEETECH_STS_CMD_READ);
}

// WRITE 写寄存器：地址/数据 -> WRITE 参数 -> 发送 WRITE 指令。
HAL_StatusTypeDef FeetechStsBus_Write(uint8_t id, uint8_t address, const uint8_t *data, uint8_t len)
{
    uint8_t params[FEETECH_STS_PARAM_MAX_LEN + 1U];    // WRITE 参数：起始地址 + 写入数据

    if ((data == 0) || (len == 0U) || (len > FEETECH_STS_PARAM_MAX_LEN)) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    params[0] = address;
    memcpy(&params[1], data, len);                     // data -> WRITE 参数区
    s_feetech_sts.pending_read_addr = 0U;              // WRITE 不需要 READ 上下文
    s_feetech_sts.pending_read_len = 0U;
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_WRITE, params, (uint8_t)(len + 1U), FEETECH_STS_CMD_WRITE);
}

// REG_WRITE 暂存写入：地址/数据 -> REG_WRITE 参数 -> 等 ACTION 生效。
HAL_StatusTypeDef FeetechStsBus_RegWrite(uint8_t id, uint8_t address, const uint8_t *data, uint8_t len)
{
    uint8_t params[FEETECH_STS_PARAM_MAX_LEN + 1U];    // REG_WRITE 参数：起始地址 + 暂存数据

    if ((data == 0) || (len == 0U) || (len > FEETECH_STS_PARAM_MAX_LEN)) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    params[0] = address;
    memcpy(&params[1], data, len);                     // data -> REG_WRITE 参数区
    s_feetech_sts.pending_read_addr = 0U;              // REG_WRITE 不需要 READ 上下文
    s_feetech_sts.pending_read_len = 0U;
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_REG_WRITE, params, (uint8_t)(len + 1U), FEETECH_STS_CMD_REG_WRITE);
}

// ACTION 执行暂存写入：发送 ACTION 指令。
HAL_StatusTypeDef FeetechStsBus_Action(uint8_t id)
{
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_ACTION, 0, 0U, FEETECH_STS_CMD_ACTION);
}

// 复位舵机状态：发送 RESET 指令。
HAL_StatusTypeDef FeetechStsBus_ResetStatus(uint8_t id)
{
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_RESET, 0, 0U, FEETECH_STS_CMD_RESET_STATUS);
}

/*============================================================
 * 5. ST-3215-C001 常用控制接口
 *============================================================*/

// 解锁 EEPROM：写 LOCK=0。
HAL_StatusTypeDef FeetechStsBus_UnlockEeprom(uint8_t id)
{
    uint8_t value;

    value = 0U;
    return FeetechStsBus_Write(id, FEETECH_STS_ADDR_LOCK, &value, 1U);
}

// 锁定 EEPROM：写 LOCK=1。
HAL_StatusTypeDef FeetechStsBus_LockEeprom(uint8_t id)
{
    uint8_t value;

    value = 1U;
    return FeetechStsBus_Write(id, FEETECH_STS_ADDR_LOCK, &value, 1U);
}

// 修改舵机 ID：检查 new_id -> 写 ID 寄存器。
HAL_StatusTypeDef FeetechStsBus_SetId(uint8_t current_id, uint8_t new_id)
{
    uint8_t value;

    if (new_id > FEETECH_STS_ID_MAX) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    value = new_id;
    return FeetechStsBus_Write(current_id, FEETECH_STS_ADDR_ID, &value, 1U);
}

// 设置波特率：baud_index -> BAUD_RATE 寄存器。
HAL_StatusTypeDef FeetechStsBus_SetBaudRate(uint8_t id, uint8_t baud_index)
{
    uint8_t value;

    value = baud_index;
    return FeetechStsBus_Write(id, FEETECH_STS_ADDR_BAUD_RATE, &value, 1U);
}

// 读取位置：PRESENT_POSITION -> feedback.position/angle。
HAL_StatusTypeDef FeetechStsBus_ReadPosition(uint8_t id)
{
    return FeetechStsBus_Read(id, FEETECH_STS_ADDR_PRESENT_POSITION_L, 2U);
}

// 读取电压温度：PRESENT_VOLTAGE 起点 -> feedback.voltage/temperature。
HAL_StatusTypeDef FeetechStsBus_ReadVoltageTemperature(uint8_t id)
{
    return FeetechStsBus_Read(id, FEETECH_STS_ADDR_PRESENT_VOLTAGE, 2U);
}

// 读取电流：PRESENT_CURRENT -> feedback.current。
HAL_StatusTypeDef FeetechStsBus_ReadCurrent(uint8_t id)
{
    return FeetechStsBus_Read(id, FEETECH_STS_ADDR_PRESENT_CURRENT_L, 2U);
}

// 设置扭矩使能：enable -> TORQUE_ENABLE 寄存器。
HAL_StatusTypeDef FeetechStsBus_SetTorque(uint8_t id, uint8_t enable)
{
    uint8_t params[2];

    params[0] = FEETECH_STS_ADDR_TORQUE_ENABLE;
    params[1] = (enable == 0U) ? FEETECH_STS_TORQUE_DISABLE : FEETECH_STS_TORQUE_ENABLE;
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_WRITE, params, sizeof(params), FEETECH_STS_CMD_SET_TORQUE);
}

// 设置扭矩限制：limit -> 限幅 -> TORQUE_LIMIT 寄存器。
HAL_StatusTypeDef FeetechStsBus_SetTorqueLimit(uint8_t id, uint16_t limit)
{
    uint8_t data[2];

    if (limit > FEETECH_STS_TORQUE_LIMIT_MAX) {
        limit = FEETECH_STS_TORQUE_LIMIT_MAX;
    }

    FeetechSts_SplitU16(&data[0], &data[1], limit);
    return FeetechStsBus_Write(id, FEETECH_STS_ADDR_TORQUE_LIMIT_L, data, sizeof(data));
}

// 设置扭矩百分比：percent -> 0~100 -> limit -> TORQUE_LIMIT。
HAL_StatusTypeDef FeetechStsBus_SetTorqueLimitPercent(uint8_t id, uint8_t percent)
{
    uint16_t limit;

    if (percent > 100U) {
        percent = 100U;
    }

    limit = (uint16_t)((uint16_t)percent * 10U);
    return FeetechStsBus_SetTorqueLimit(id, limit);
}

// 使能扭矩：TORQUE_ENABLE=1。
HAL_StatusTypeDef FeetechStsBus_EnableTorque(uint8_t id)
{
    return FeetechStsBus_SetTorque(id, FEETECH_STS_TORQUE_ENABLE);
}

// 关闭扭矩：TORQUE_ENABLE=0。
HAL_StatusTypeDef FeetechStsBus_DisableTorque(uint8_t id)
{
    return FeetechStsBus_SetTorque(id, FEETECH_STS_TORQUE_DISABLE);
}

// 设置运行模式：mode -> MODE 寄存器。
HAL_StatusTypeDef FeetechStsBus_SetMode(uint8_t id, uint8_t mode)
{
    uint8_t params[2];

    params[0] = FEETECH_STS_ADDR_MODE;
    params[1] = mode;
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_WRITE, params, sizeof(params), FEETECH_STS_CMD_SET_MODE);
}

// 设置目标位置：position/time/speed -> 目标寄存器 -> 更新 motion。
HAL_StatusTypeDef FeetechStsBus_SetPosition(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed)
{
    uint8_t data[6];

    position = FeetechSts_LimitPosition(position);
    time_ms = FeetechSts_LimitTime(time_ms);
    speed = FeetechSts_LimitSpeed(speed);

    FeetechSts_SplitU16(&data[0], &data[1], position);
    FeetechSts_SplitU16(&data[2], &data[3], time_ms);
    FeetechSts_SplitU16(&data[4], &data[5], speed);

    s_feetech_sts.bus.motion.id = id;
    s_feetech_sts.bus.motion.target_position = position;
    s_feetech_sts.bus.motion.target_angle_deg = FeetechSts_PositionToAngle(position);
    s_feetech_sts.bus.motion.target_time_ms = time_ms;
    s_feetech_sts.bus.motion.target_speed = speed;

    {
        uint8_t params[7];

        params[0] = FEETECH_STS_ADDR_GOAL_POSITION_L;
        memcpy(&params[1], data, sizeof(data));
        return FeetechSts_SendPacket(id, FEETECH_STS_INST_WRITE, params, sizeof(params), FEETECH_STS_CMD_SET_TARGET);
    }
}

// 设置目标角度：angle -> position -> 设置目标位置。
HAL_StatusTypeDef FeetechStsBus_SetAngle(uint8_t id, float angle_deg, uint16_t time_ms, uint16_t speed)
{
    return FeetechStsBus_SetPosition(id, FeetechSts_AngleToPosition(angle_deg), time_ms, speed);
}

// 设置扩展目标位置：acc/position/time/speed -> 扩展目标寄存器 -> 更新 motion。
HAL_StatusTypeDef FeetechStsBus_SetPositionEx(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    uint8_t data[7];

    position = FeetechSts_LimitPosition(position);
    time_ms = FeetechSts_LimitTime(time_ms);
    speed = FeetechSts_LimitSpeed(speed);

    data[0] = acc;
    FeetechSts_SplitU16(&data[1], &data[2], position);
    FeetechSts_SplitU16(&data[3], &data[4], time_ms);
    FeetechSts_SplitU16(&data[5], &data[6], speed);

    s_feetech_sts.bus.motion.id = id;
    s_feetech_sts.bus.motion.target_position = position;
    s_feetech_sts.bus.motion.target_angle_deg = FeetechSts_PositionToAngle(position);
    s_feetech_sts.bus.motion.target_time_ms = time_ms;
    s_feetech_sts.bus.motion.target_speed = speed;
    s_feetech_sts.bus.motion.target_acc = acc;

    {
        uint8_t params[8];

        params[0] = FEETECH_STS_ADDR_ACC;
        memcpy(&params[1], data, sizeof(data));
        return FeetechSts_SendPacket(id, FEETECH_STS_INST_WRITE, params, sizeof(params), FEETECH_STS_CMD_SET_TARGET_EX);
    }
}

// 设置扩展目标角度：angle -> position -> 设置扩展目标位置。
HAL_StatusTypeDef FeetechStsBus_SetAngleEx(uint8_t id, float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    return FeetechStsBus_SetPositionEx(id, FeetechSts_AngleToPosition(angle_deg), time_ms, speed, acc);
}

// 读取综合反馈：反馈起点/长度 -> READ -> 等待解析到 feedback。
HAL_StatusTypeDef FeetechStsBus_ReadFeedback(uint8_t id)
{
    uint8_t params[2];

    params[0] = FEETECH_STS_FEEDBACK_READ_ADDR;
    params[1] = FEETECH_STS_FEEDBACK_READ_LEN;
    s_feetech_sts.pending_read_addr = FEETECH_STS_FEEDBACK_READ_ADDR;
    s_feetech_sts.pending_read_len = FEETECH_STS_FEEDBACK_READ_LEN;
    return FeetechSts_SendPacket(id, FEETECH_STS_INST_READ, params, sizeof(params), FEETECH_STS_CMD_READ_FEEDBACK);
}

// 阻塞读取反馈：清旧标志 -> 发送 READ -> 等待反馈更新或超时。
HAL_StatusTypeDef FeetechStsBus_ReadFeedbackBlocking(uint8_t id, uint32_t timeout_ms)
{
    uint32_t start_tick;
    uint32_t rx_packet_count_before;
    HAL_StatusTypeDef status;
    FeetechStsFeedback_t *feedback;
    uint8_t flags;

    if (timeout_ms == 0U) {
        timeout_ms = FEETECH_STS_RESP_TIMEOUT_MS;
    }

    feedback = &s_feetech_sts.bus.feedback;
    rx_packet_count_before = feedback->rx_packet_count;
    (void)FeetechStsBus_TakeUpdateFlags();

    status = FeetechStsBus_ReadFeedback(id);
    if (status != HAL_OK) {
        return status;
    }

    start_tick = HAL_GetTick();
    do {
        flags = FeetechStsBus_TakeUpdateFlags();
        if (((flags & FEETECH_STS_UPDATE_POSITION) != 0U) &&
            (feedback->rx_packet_count != rx_packet_count_before) &&
            (feedback->id == id) &&
            (feedback->position_valid != 0U)) {
            return HAL_OK;
        }
    } while ((uint32_t)(HAL_GetTick() - start_tick) <= timeout_ms);

    return HAL_TIMEOUT;
}
// 同步写多个目标：多个 ID/position/time/speed -> SYNC_WRITE 帧。
HAL_StatusTypeDef FeetechStsBus_SyncWritePosition(uint8_t *ids, uint8_t id_count, uint16_t *positions, uint16_t *times_ms, uint16_t *speeds)
{
    uint8_t params[FEETECH_STS_PARAM_MAX_LEN];
    uint8_t i;
    uint8_t pos;
    uint16_t value;

    if ((ids == 0) || (positions == 0) || (id_count == 0U)) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    if ((uint16_t)(2U + ((uint16_t)id_count * 7U)) > FEETECH_STS_PARAM_MAX_LEN) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    params[0] = FEETECH_STS_ADDR_GOAL_POSITION_L;
    params[1] = 6U;
    pos = 2U;

    for (i = 0U; i < id_count; i++) {
        params[pos++] = ids[i];

        value = FeetechSts_LimitPosition(positions[i]);
        FeetechSts_SplitU16(&params[pos], &params[(uint8_t)(pos + 1U)], value);
        pos = (uint8_t)(pos + 2U);

        value = (times_ms == 0) ? FEETECH_STS_DEFAULT_TIME_MS : FeetechSts_LimitTime(times_ms[i]);
        FeetechSts_SplitU16(&params[pos], &params[(uint8_t)(pos + 1U)], value);
        pos = (uint8_t)(pos + 2U);

        value = (speeds == 0) ? FEETECH_STS_DEFAULT_SPEED : FeetechSts_LimitSpeed(speeds[i]);
        FeetechSts_SplitU16(&params[pos], &params[(uint8_t)(pos + 1U)], value);
        pos = (uint8_t)(pos + 2U);
    }

    return FeetechSts_SendPacket(FEETECH_STS_BROADCAST_ID, FEETECH_STS_INST_SYNC_WRITE, params, pos, FEETECH_STS_CMD_SYNC_WRITE_TARGET);
}

/*============================================================
 * 6. 项目运动目标接口
 *============================================================*/

// 设置运动目标：角度/时间/速度 -> 默认加速度 -> 扩展目标缓存。
void FeetechStsMotion_SetTarget(float angle_deg, uint16_t time_ms, uint16_t speed)
{
    FeetechStsMotion_SetTargetEx(angle_deg, time_ms, speed, FEETECH_STS_DEFAULT_ACC);
}

// 设置扩展运动目标：角度/时间/速度/加速度 -> motion 缓存。
void FeetechStsMotion_SetTargetEx(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    s_feetech_sts.bus.motion.id = FEETECH_STS_DEFAULT_ID;
    s_feetech_sts.bus.motion.target_angle_deg = FeetechSts_LimitAngle(angle_deg);
    s_feetech_sts.bus.motion.target_position = FeetechSts_AngleToPosition(s_feetech_sts.bus.motion.target_angle_deg);
    s_feetech_sts.bus.motion.target_time_ms = FeetechSts_LimitTime(time_ms);
    s_feetech_sts.bus.motion.target_speed = FeetechSts_LimitSpeed(speed);
    s_feetech_sts.bus.motion.target_acc = acc;

    FeetechSts_UpdateReached();    // 更新目标后立即刷新到位状态
}

// 运动目标任务：检查目标变化/周期重发 -> 下发目标 -> 更新到位状态。
void FeetechStsMotion_Task(void)
{
    uint32_t now;
    uint8_t need_send;
    FeetechStsMotion_t *motion;

    now = HAL_GetTick();
    motion = &s_feetech_sts.bus.motion;

    need_send = 0U;
    if (s_feetech_sts.motion_sent_valid == 0U) {                                                  // 目标尚未发送
        need_send = 1U;
    } else if (motion->target_angle_deg != s_feetech_sts.last_sent_angle_deg) {                    // 目标角度变化
        need_send = 1U;
    } else if (motion->target_time_ms != s_feetech_sts.last_sent_time_ms) {                         // 目标时间变化
        need_send = 1U;
    } else if (motion->target_speed != s_feetech_sts.last_sent_speed) {                             // 目标速度变化
        need_send = 1U;
    } else if (motion->target_acc != s_feetech_sts.last_sent_acc) {                                 // 目标加速度变化
        need_send = 1U;
    } else if ((uint32_t)(now - s_feetech_sts.last_motion_send_tick) >= FEETECH_STS_MOTION_SEND_PERIOD_MS) { // 到达周期重发时间
        need_send = 1U;
    }

    if (need_send != 0U) {
        (void)FeetechStsBus_SetAngleEx(motion->id,
                                       motion->target_angle_deg,
                                       motion->target_time_ms,
                                       motion->target_speed,
                                       motion->target_acc);     // motion 目标 -> 舵机目标寄存器

        s_feetech_sts.motion_sent_valid = 1U;                   // 标记当前目标已发送
        s_feetech_sts.last_sent_angle_deg = motion->target_angle_deg;
        s_feetech_sts.last_sent_time_ms = motion->target_time_ms;
        s_feetech_sts.last_sent_speed = motion->target_speed;
        s_feetech_sts.last_sent_acc = motion->target_acc;
        s_feetech_sts.last_motion_send_tick = now;
    }

    FeetechSts_UpdateReached();                                 // 当前反馈/目标 -> 到位状态
}

/*============================================================
 * 7. 角度、位置和字节序工具
 *============================================================*/

// 角度转位置：angle_deg -> 限幅 -> 0~4095 位置值。
uint16_t FeetechSts_AngleToPosition(float angle_deg)
{
    float limited_angle;
    float position_float;

    limited_angle = FeetechSts_LimitAngle(angle_deg);
    position_float = (limited_angle * (float)FEETECH_STS_POSITION_FULL_SCALE) / 360.0f;

    if (position_float < 0.0f) {
        return FEETECH_STS_POSITION_MIN;
    }
    if (position_float > (float)FEETECH_STS_POSITION_MAX) {
        return FEETECH_STS_POSITION_MAX;
    }

    return (uint16_t)(position_float + 0.5f);
}

// 位置转角度：0~4095 位置值 -> 限幅 -> 角度。
float FeetechSts_PositionToAngle(uint16_t position)
{
    uint16_t limited_position;

    limited_position = FeetechSts_LimitPosition(position);
    return ((float)limited_position * 360.0f) / (float)FEETECH_STS_POSITION_FULL_SCALE;
}

// 限制位置：输入位置 -> 协议允许范围。
uint16_t FeetechSts_LimitPosition(uint16_t position)
{
    if (position > FEETECH_STS_POSITION_MAX) {
        return FEETECH_STS_POSITION_MAX;
    }
    return position;
}

// 限制角度：输入角度 -> 项目机械允许范围。
float FeetechSts_LimitAngle(float angle_deg)
{
    if (angle_deg < FEETECH_STS_ANGLE_MIN_DEG) {
        return FEETECH_STS_ANGLE_MIN_DEG;
    }
    if (angle_deg > FEETECH_STS_ANGLE_MAX_DEG) {
        return FEETECH_STS_ANGLE_MAX_DEG;
    }
    return angle_deg;
}

// 合成 U16：低字节/高字节 -> uint16_t。
uint16_t FeetechSts_MakeU16(uint8_t low, uint8_t high)
{
    return (uint16_t)low | ((uint16_t)high << 8);
}

// 拆分 U16：uint16_t -> 低字节/高字节。
void FeetechSts_SplitU16(uint8_t *low, uint8_t *high, uint16_t value)
{
    if ((low == 0) || (high == 0)) {
        return;
    }

    *low = (uint8_t)(value & 0xFFU);
    *high = (uint8_t)((value >> 8) & 0xFFU);
}

/*============================================================
 * 8. 内部组帧和发送
 *============================================================*/

// 组帧并发送：ID/指令/参数 -> STS 帧 -> UART2 发送。
static HAL_StatusTypeDef FeetechSts_SendPacket(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len, FeetechStsCommand_t command)
{
    uint8_t packet[FEETECH_STS_FRAME_MAX_LEN];
    uint8_t packet_len;
    HAL_StatusTypeDef status;
    uint8_t address;
    uint8_t data_len;

    packet_len = FeetechSts_BuildPacket(id, instruction, params, param_len, packet, sizeof(packet));
    if (packet_len == 0U) {
        s_feetech_sts.bus.last_status = HAL_ERROR;
        return HAL_ERROR;
    }

    address = 0U;
    data_len = 0U;
    if ((params != 0) && (param_len > 0U)) {
        address = params[0];
        if (param_len > 1U) {
            data_len = (uint8_t)(param_len - 1U);
        }
    }

    FeetechSts_RecordTx(command, id, instruction, address, data_len, packet, packet_len);
    FeetechSts_ResetRxParser();
    FeetechStsPort_ClearOverrun();

    status = FeetechStsPort_Transmit(
        packet,
        packet_len,
        FEETECH_STS_TX_TIMEOUT_MS);
    s_feetech_sts.bus.last_status = status;
    if (status == HAL_OK) {
        s_feetech_sts.bus.tx.tx_count++;
    }

    (void)FeetechStsBus_StartReceive();

    return status;
}

// 构造 STS 帧：ID/指令/参数 -> 帧头/长度/校验。
uint8_t FeetechSts_BuildPacket(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len, uint8_t *out, uint8_t out_max)
{
    uint8_t i;
    uint8_t length;
    uint8_t total_len;

    length = (uint8_t)(param_len + 2U);
    total_len = (uint8_t)(param_len + 6U);

    if ((out == 0) || (total_len > out_max) || (total_len > FEETECH_STS_FRAME_MAX_LEN)) {
        return 0U;
    }
    if ((param_len > 0U) && (params == 0)) {
        return 0U;
    }

    out[0] = FEETECH_STS_FRAME_HEADER;
    out[1] = FEETECH_STS_FRAME_HEADER;
    out[2] = id;
    out[3] = length;
    out[4] = instruction;

    for (i = 0U; i < param_len; i++) {
        out[(uint8_t)(5U + i)] = params[i];
    }

    out[(uint8_t)(total_len - 1U)] = FeetechSts_CalcChecksum(out, total_len);
    return total_len;
}

// 计算校验：ID 到最后参数求和取反 -> CheckSum。
uint8_t FeetechSts_CalcChecksum(const uint8_t *packet, uint8_t total_len)
{
    uint16_t sum;
    uint8_t i;

    if ((packet == 0) || (total_len < 6U)) {
        return 0U;
    }

    sum = 0U;
    for (i = 2U; i < (uint8_t)(total_len - 1U); i++) {
        sum = (uint16_t)(sum + packet[i]);
    }

    return (uint8_t)(~((uint8_t)sum));
}

// 校验 STS 帧：重新计算 CheckSum -> 对比帧尾校验字节。
uint8_t FeetechSts_VerifyChecksum(const uint8_t *packet, uint8_t total_len)
{
    if ((packet == 0) || (total_len < 6U)) {
        return 0U;
    }

    return (FeetechSts_CalcChecksum(packet, total_len) == packet[(uint8_t)(total_len - 1U)]) ? 1U : 0U;
}

// 计算整帧长度：帧头/Len 字段 -> total_len。
uint8_t FeetechSts_GetFrameTotalLen(const uint8_t *packet, uint8_t current_len)
{
    uint8_t total_len;

    if ((packet == 0) || (current_len < 4U)) {
        return 0U;
    }
    if ((packet[0] != FEETECH_STS_FRAME_HEADER) || (packet[1] != FEETECH_STS_FRAME_HEADER)) {
        return 0U;
    }

    total_len = (uint8_t)(packet[3] + 4U);
    if ((total_len < 6U) || (total_len > FEETECH_STS_FRAME_MAX_LEN)) {
        return 0U;
    }

    return total_len;
}

// 检查 STS 帧：帧头 -> 长度 -> 校验。
uint8_t FeetechSts_IsValidFrame(const uint8_t *packet, uint8_t total_len)
{
    uint8_t expected_len;

    if ((packet == 0) || (total_len < 6U) || (total_len > FEETECH_STS_FRAME_MAX_LEN)) {
        return 0U;
    }
    if ((packet[0] != FEETECH_STS_FRAME_HEADER) || (packet[1] != FEETECH_STS_FRAME_HEADER)) {
        return 0U;
    }

    expected_len = FeetechSts_GetFrameTotalLen(packet, total_len);
    if (expected_len != total_len) {
        return 0U;
    }

    return FeetechSts_VerifyChecksum(packet, total_len);
}

// 记录发送上下文：命令信息/原始帧 -> tx 状态 -> pending_command。
static void FeetechSts_RecordTx(FeetechStsCommand_t command, uint8_t id, uint8_t instruction, uint8_t address, uint8_t data_len, const uint8_t *frame, uint8_t frame_len)
{
    FeetechStsTxState_t *tx;

    tx = &s_feetech_sts.bus.tx;
    tx->command = command;
    tx->id = id;
    tx->instruction = instruction;
    tx->address = address;
    tx->data_len = data_len;
    tx->frame_len = frame_len;
    tx->tick_ms = HAL_GetTick();

    if ((frame != 0) && (frame_len <= FEETECH_STS_FRAME_MAX_LEN)) {
        memcpy(tx->frame, frame, frame_len);
    }

    s_feetech_sts.pending_command = command;
}

// 映射指令类型：STS 指令码 -> 内部命令类型。
static FeetechStsCommand_t FeetechSts_MapInstruction(uint8_t instruction)
{
    switch (instruction) {
        case FEETECH_STS_INST_PING:
            return FEETECH_STS_CMD_PING;
        case FEETECH_STS_INST_READ:
            return FEETECH_STS_CMD_READ;
        case FEETECH_STS_INST_WRITE:
            return FEETECH_STS_CMD_WRITE;
        case FEETECH_STS_INST_REG_WRITE:
            return FEETECH_STS_CMD_REG_WRITE;
        case FEETECH_STS_INST_ACTION:
            return FEETECH_STS_CMD_ACTION;
        case FEETECH_STS_INST_SYNC_WRITE:
            return FEETECH_STS_CMD_SYNC_WRITE_TARGET;
        case FEETECH_STS_INST_RESET:
            return FEETECH_STS_CMD_RESET_STATUS;
        default:
            return FEETECH_STS_CMD_NONE;
    }
}

// 判断是否等待回包：广播非 PING -> 不等待，其余默认等待。
static uint8_t FeetechSts_FrameExpectResponse(const uint8_t *frame, uint8_t len)
{
    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }

    if ((frame[2] == FEETECH_STS_BROADCAST_ID) && (frame[4] != FEETECH_STS_INST_PING)) {
        return 0U;
    }

    return 1U;
}

/*============================================================
 * 9. 内部接收解析
 *============================================================*/

// 复位接收解析器：清已收长度 -> 清期望长度。
static void FeetechSts_ResetRxParser(void)
{
    s_feetech_sts.rx_index = 0U;
    s_feetech_sts.rx_expected_len = 0U;
}

// 接收字节 -> 状态包组帧：帧头 -> 长度 -> 收满后解析。
static void FeetechSts_ProcessRxByte(uint8_t data)
{
    if (s_feetech_sts.rx_index == 0U) {
        if (data != FEETECH_STS_FRAME_HEADER) {
            return;
        }
        s_feetech_sts.rx_frame[s_feetech_sts.rx_index++] = data;
        return;
    }

    if (s_feetech_sts.rx_index == 1U) {
        if (data != FEETECH_STS_FRAME_HEADER) {
            FeetechSts_ResetRxParser();
            if (data == FEETECH_STS_FRAME_HEADER) {
                s_feetech_sts.rx_frame[s_feetech_sts.rx_index++] = data;
            }
            return;
        }
        s_feetech_sts.rx_frame[s_feetech_sts.rx_index++] = data;
        return;
    }

    if (s_feetech_sts.rx_index >= FEETECH_STS_FRAME_MAX_LEN) {
        s_feetech_sts.bus.feedback.parse_error_count++;
        FeetechSts_ResetRxParser();
        return;
    }

    s_feetech_sts.rx_frame[s_feetech_sts.rx_index++] = data;

    if (s_feetech_sts.rx_index == 4U) {
        s_feetech_sts.rx_expected_len = (uint8_t)(s_feetech_sts.rx_frame[3] + 4U);
        if ((s_feetech_sts.rx_expected_len < 6U) || (s_feetech_sts.rx_expected_len > FEETECH_STS_FRAME_MAX_LEN)) {
            s_feetech_sts.bus.feedback.parse_error_count++;
            FeetechSts_ResetRxParser();
            return;
        }
    }

    if ((s_feetech_sts.rx_expected_len != 0U) && (s_feetech_sts.rx_index >= s_feetech_sts.rx_expected_len)) {
        FeetechSts_ParseStatusPacket(s_feetech_sts.rx_frame, s_feetech_sts.rx_expected_len);
        FeetechSts_ResetRxParser();
    }
}

// 解析状态包：校验原始回包 -> 缓存 packet -> 分类更新 feedback。
static void FeetechSts_ParseStatusPacket(const uint8_t *packet, uint8_t total_len)
{
    FeetechStsPacket_t *last_packet;
    FeetechStsFeedback_t *feedback;
    uint8_t length;
    uint8_t param_len;

    feedback = &s_feetech_sts.bus.feedback;

    if ((packet == 0) || (total_len < 6U)) {
        feedback->parse_error_count++;
        return;
    }
    if ((packet[0] != FEETECH_STS_FRAME_HEADER) || (packet[1] != FEETECH_STS_FRAME_HEADER)) {
        feedback->parse_error_count++;
        return;
    }
    if (FeetechSts_VerifyChecksum(packet, total_len) == 0U) {              // 校验失败 -> 丢弃状态包
        feedback->parse_error_count++;
        return;
    }

    length = packet[3];
    if ((uint8_t)(length + 4U) != total_len) {
        feedback->parse_error_count++;
        return;
    }

    param_len = (uint8_t)(length - 2U);
    if (param_len > FEETECH_STS_PARAM_MAX_LEN) {
        feedback->parse_error_count++;
        return;
    }
    
    /* PING ERROR=1 is byte-identical to its request. Keep that ambiguous
     * candidate for identity READ confirmation. Other identical frames are echoes. */
    if ((total_len == s_feetech_sts.bus.tx.frame_len) &&
        (total_len > 0U) &&
        (memcmp(packet,
                s_feetech_sts.bus.tx.frame,
                total_len) == 0) &&
        !((s_feetech_sts.pending_command == FEETECH_STS_CMD_PING) &&
          (total_len == 6U))) {
        s_feetech_sts.tx_echo_drop_count++;
        return;
    }

    last_packet = &s_feetech_sts.bus.packet;
    memset(last_packet, 0, sizeof(*last_packet));
    last_packet->id = packet[2];
    last_packet->error = packet[4];
    last_packet->param_len = param_len;
    last_packet->raw_len = total_len;
    last_packet->tick_ms = HAL_GetTick();
    last_packet->type = (param_len == 0U) ? FEETECH_STS_PACKET_ACK : FEETECH_STS_PACKET_READ_DATA;
    last_packet->unread = 1U;
    memcpy(last_packet->raw, packet, total_len);
    if (param_len > 0U) {
        memcpy(last_packet->params, &packet[5], param_len);
    }

    /* An ambiguous PING candidate alone cannot establish online feedback. */
    if ((s_feetech_sts.pending_command == FEETECH_STS_CMD_PING) &&
        (total_len == 6U) && (packet[4] == FEETECH_STS_INST_PING)) return;

    feedback->id = packet[2];
    feedback->error = packet[4];
    feedback->online = 1U;
    feedback->rx_packet_count++;
    feedback->last_update_tick = last_packet->tick_ms;
    feedback->update_flags |= (FEETECH_STS_UPDATE_RAW | FEETECH_STS_UPDATE_STATUS);

    if ((param_len > 0U) &&
        ((s_feetech_sts.pending_command == FEETECH_STS_CMD_READ) ||
         (s_feetech_sts.pending_command == FEETECH_STS_CMD_READ_FEEDBACK)) &&
        (param_len == s_feetech_sts.pending_read_len)) {
        // ERROR 是舵机告警；READ 参数完整时仍更新真实反馈。
        FeetechSts_ApplyReadData(s_feetech_sts.pending_read_addr, &packet[5], param_len);
    }

    FeetechSts_UpdateReached();
}

// 应用 READ 数据：pending 地址 + params -> feedback 字段。
static void FeetechSts_ApplyReadData(uint8_t address, const uint8_t *params, uint8_t len)
{
    FeetechStsFeedback_t *feedback;
    uint8_t offset;

    if ((params == 0) || (len == 0U)) {
        return;
    }

    feedback = &s_feetech_sts.bus.feedback;

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_POSITION_L, 2U, &offset) != 0U) {
        feedback->position = FeetechSts_MakeU16(params[offset], params[(uint8_t)(offset + 1U)]);
        feedback->angle_deg = FeetechSts_PositionToAngle(feedback->position);
        feedback->position_valid = 1U;
        feedback->update_flags |= FEETECH_STS_UPDATE_POSITION;
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_SPEED_L, 2U, &offset) != 0U) {
        feedback->speed = FeetechSts_MakeS16(params[offset], params[(uint8_t)(offset + 1U)]);
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_LOAD_L, 2U, &offset) != 0U) {
        feedback->load = FeetechSts_MakeS16(params[offset], params[(uint8_t)(offset + 1U)]);
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_VOLTAGE, 1U, &offset) != 0U) {
        feedback->voltage_raw = params[offset];
        feedback->voltage_mv = (uint16_t)params[offset] * 100U;
        feedback->telemetry_valid = 1U;
        feedback->update_flags |= FEETECH_STS_UPDATE_TELEMETRY;
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_TEMPERATURE, 1U, &offset) != 0U) {
        feedback->temperature_c = params[offset];
        feedback->telemetry_valid = 1U;
        feedback->update_flags |= FEETECH_STS_UPDATE_TELEMETRY;
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_MOVING, 1U, &offset) != 0U) {
        feedback->moving = params[offset];
        feedback->update_flags |= FEETECH_STS_UPDATE_STATUS;
    }

    if (FeetechSts_FieldAvailable(address, len, FEETECH_STS_ADDR_PRESENT_CURRENT_L, 2U, &offset) != 0U) {
        feedback->current_raw = FeetechSts_MakeS16(params[offset], params[(uint8_t)(offset + 1U)]);
        if (feedback->current_raw >= 0) {
            feedback->current_ma = (uint16_t)(((uint32_t)feedback->current_raw * FEETECH_STS_CURRENT_UNIT_MA_X10) / 10UL);
        } else {
            feedback->current_ma = (uint16_t)(((uint32_t)(-feedback->current_raw) * FEETECH_STS_CURRENT_UNIT_MA_X10) / 10UL);
        }
        feedback->telemetry_valid = 1U;
        feedback->update_flags |= FEETECH_STS_UPDATE_TELEMETRY;
    }
}

// 判断字段是否在 READ 范围内：base/len + field/field_len -> offset。
static uint8_t FeetechSts_FieldAvailable(uint8_t base, uint8_t len, uint8_t field, uint8_t field_len, uint8_t *offset)
{
    uint16_t start;
    uint16_t end;
    uint16_t field_start;
    uint16_t field_end;

    if (offset == 0) {
        return 0U;
    }

    start = base;
    end = (uint16_t)base + (uint16_t)len;
    field_start = field;
    field_end = (uint16_t)field + (uint16_t)field_len;

    if ((field_start < start) || (field_end > end)) {
        return 0U;
    }

    *offset = (uint8_t)(field_start - start);
    return 1U;
}

/*============================================================
 * 10. 内部辅助函数
 *============================================================*/

// 合成 S16：低字节/高字节 -> int16_t。
static int16_t FeetechSts_MakeS16(uint8_t low, uint8_t high)
{
    return (int16_t)FeetechSts_MakeU16(low, high);
}

// 限制目标时间：time_ms -> 0~FEETECH_STS_TIME_MAX_MS。
static uint16_t FeetechSts_LimitTime(uint16_t time_ms)
{
    if (time_ms > FEETECH_STS_TIME_MAX_MS) {
        return FEETECH_STS_TIME_MAX_MS;
    }
    return time_ms;
}

// 限制目标速度：speed -> 0~FEETECH_STS_SPEED_MAX。
static uint16_t FeetechSts_LimitSpeed(uint16_t speed)
{
    if (speed > FEETECH_STS_SPEED_MAX) {
        return FEETECH_STS_SPEED_MAX;
    }
    return speed;
}

// 计算绝对差：a/b -> |a-b|。
static float FeetechSts_AbsFloatDiff(float a, float b)
{
    return (a >= b) ? (a - b) : (b - a);
}

// 更新到位状态：目标角度/反馈角度 -> motion.reached。
static void FeetechSts_UpdateReached(void)
{
    FeetechStsMotion_t *motion;
    FeetechStsFeedback_t *feedback;

    motion = &s_feetech_sts.bus.motion;
    feedback = &s_feetech_sts.bus.feedback;

    if (feedback->position_valid == 0U) {
        motion->reached = 0U;
        return;
    }

    motion->reached = (FeetechSts_AbsFloatDiff(motion->target_angle_deg, feedback->angle_deg) <= FEETECH_STS_REACHED_MARGIN_DEG) ? 1U : 0U;
}
