/*********************************************************************************************************************
 * @file    feetech_sts_bus.h
 * @brief   飞特 STS 总线协议接口。
 *
 * @details
 * 1. 本文件负责：定义协议常量、反馈缓存、目标缓存和底层收发接口。
 * 2. 主要流程：WRITE 写寄存器；READ 回包更新反馈。
 *********************************************************************************************************************/

#ifndef __FEETECH_STS_BUS_H__
#define __FEETECH_STS_BUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "board_config.h"
#include <stdint.h>

/*============================================================
 * 1. 协议和硬件基础配置
 *============================================================*/

#define FEETECH_STS_DEFAULT_ID                  BOARD_SERVO_ID /* Single board-level target; upstream uses 1. */
#define FEETECH_STS_BROADCAST_ID                0xFEU       /* 广播 ID，除 PING 外一般不返回应答包。 */
#define FEETECH_STS_ID_MIN                      0U          /* 普通 ID 最小值。 */
#define FEETECH_STS_ID_MAX                      253U        /* 普通 ID 最大值，254 为广播 ID。 */

#define FEETECH_STS_FRAME_HEADER                0xFFU       /* 协议帧头字节。 */
#define FEETECH_STS_FRAME_MAX_LEN               80U         /* 本库支持的最大完整帧长度。 */
#define FEETECH_STS_PARAM_MAX_LEN               64U         /* 单帧最大参数缓存长度。 */
#define FEETECH_STS_TX_TIMEOUT_MS               20U         /* HAL 阻塞发送超时时间。 */
#define FEETECH_STS_RESP_TIMEOUT_MS             60U         /* 官方帧透传后等待舵机应答的默认超时时间。 */
#define FEETECH_STS_RX_RESTART_LIMIT            3U          /* 接收忙时重新挂起的保护次数。 */

/*============================================================
 * 2. 官方协议指令码
 *============================================================*/

#define FEETECH_STS_INST_PING                   0x01U       /* 查询舵机在线状态。 */
#define FEETECH_STS_INST_READ                   0x02U       /* 从控制表读取数据。 */
#define FEETECH_STS_INST_WRITE                  0x03U       /* 写控制表并立即执行。 */
#define FEETECH_STS_INST_REG_WRITE              0x04U       /* 异步写控制表，等待 ACTION 后执行。 */
#define FEETECH_STS_INST_ACTION                 0x05U       /* 执行异步写入动作。 */
#define FEETECH_STS_INST_RECOVERY               0x06U       /* 恢复参数，危险操作，常规流程不调用。 */
#define FEETECH_STS_INST_SYNC_READ              0x82U       /* 同步读取多个舵机。 */
#define FEETECH_STS_INST_SYNC_WRITE             0x83U       /* 同步写入多个舵机。 */
#define FEETECH_STS_INST_RESET                  0x0AU       /* 重置舵机状态，例如清圈数。 */

/*============================================================
 * 3. ST-3215-C001 / SMS_STS 常用控制表地址
 *============================================================*/

#define FEETECH_STS_ADDR_MODEL_L                3U          /* 型号低字节，只读。 */
#define FEETECH_STS_ADDR_MODEL_H                4U          /* 型号高字节，只读。 */
#define FEETECH_STS_ADDR_ID                     5U          /* 舵机 ID，EEPROM，修改前需解锁。 */
#define FEETECH_STS_ADDR_BAUD_RATE              6U          /* 波特率索引，EEPROM。 */
#define FEETECH_STS_ADDR_MIN_ANGLE_L            9U          /* 最小角度限制低字节。 */
#define FEETECH_STS_ADDR_MAX_ANGLE_L            11U         /* 最大角度限制低字节。 */
#define FEETECH_STS_ADDR_CW_DEAD                26U         /* 顺时针死区。 */
#define FEETECH_STS_ADDR_CCW_DEAD               27U         /* 逆时针死区。 */
#define FEETECH_STS_ADDR_OFFSET_L               31U         /* 中位偏移低字节。 */
#define FEETECH_STS_ADDR_MODE                   33U         /* 运行模式，0 位置模式，1 恒速模式。 */
#define FEETECH_STS_ADDR_MAX_TORQUE_L           16U         /* EEPROM 最大扭矩低字节，危险配置项，常规运行不改。 */

#define FEETECH_STS_ADDR_TORQUE_ENABLE          40U         /* 扭矩使能，0 卸力，1 上力。 */
#define FEETECH_STS_ADDR_ACC                    41U         /* 加速度，官方 WritePosEx 从该地址开始写 7 字节。 */
#define FEETECH_STS_ADDR_GOAL_POSITION_L        42U         /* 目标位置低字节，协议手册示例地址 0x2A。 */
#define FEETECH_STS_ADDR_GOAL_TIME_L            44U         /* 运行时间低字节。 */
#define FEETECH_STS_ADDR_GOAL_SPEED_L           46U         /* 运行速度低字节。 */
#define FEETECH_STS_ADDR_TORQUE_LIMIT_L         48U         /* SRAM 转矩限制低字节，0~1000 对应 0~100%。 */
#define FEETECH_STS_ADDR_LOCK                   55U         /* EEPROM 锁，0 解锁，1 加锁。 */

#define FEETECH_STS_ADDR_PRESENT_POSITION_L     56U         /* 当前位置低字节，协议手册示例地址 0x38。 */
#define FEETECH_STS_ADDR_PRESENT_SPEED_L        58U         /* 当前速度低字节。 */
#define FEETECH_STS_ADDR_PRESENT_LOAD_L         60U         /* 当前负载低字节。 */
#define FEETECH_STS_ADDR_PRESENT_VOLTAGE        62U         /* 当前电压，单位通常为 0.1V。 */
#define FEETECH_STS_ADDR_PRESENT_TEMPERATURE    63U         /* 当前温度，单位摄氏度。 */
#define FEETECH_STS_ADDR_MOVING                 66U         /* 运动状态，非 0 表示正在运动。 */
#define FEETECH_STS_ADDR_PRESENT_CURRENT_L      69U         /* 当前电流低字节。 */

/*============================================================
 * 4. 项目默认运动约定
 *============================================================*/

#define FEETECH_STS_MODE_POSITION               0U          /* 位置模式。 */
#define FEETECH_STS_MODE_WHEEL                  1U          /* 恒速轮模式。 */
#define FEETECH_STS_TORQUE_DISABLE              0U          /* 卸力。 */
#define FEETECH_STS_TORQUE_ENABLE               1U          /* 上力。 */
#define FEETECH_STS_TORQUE_LIMIT_MAX            1000U       /* 转矩限制最大值，1000 约对应 100%。 */
#define FEETECH_STS_TORQUE_LIMIT_MIN            0U          /* 转矩限制最小值。 */
#define FEETECH_STS_CURRENT_UNIT_MA_X10         65U         /* 当前电流单位 6.5mA，放大 10 倍保存。 */

#define FEETECH_STS_POSITION_MIN                0U          // 位置下限：磁编码最小计数
#define FEETECH_STS_POSITION_MAX                4095U       // 位置上限：12 位磁编码最大计数
#define FEETECH_STS_POSITION_FULL_SCALE         4096U       // 位置满量程：4096 计数 -> 360 度

#define FEETECH_STS_ANGLE_MIN_DEG               0.0f        // 项目角度下限：夹爪最小目标角度
#define FEETECH_STS_ANGLE_MAX_DEG               270.0f      // 项目角度上限：B0 扫描使用 0~270 度

#define FEETECH_STS_DEFAULT_TIME_MS             300U        // 默认动作时间：未指定时间时使用
#define FEETECH_STS_DEFAULT_SPEED               0U          // 默认速度：0 表示按时间/舵机内部策略运行
#define FEETECH_STS_DEFAULT_ACC                 0U          // 默认加速度：0 表示不额外限制加速度
#define FEETECH_STS_SPEED_MAX                   4095U       // 速度上限：飞特速度字段最大值
#define FEETECH_STS_TIME_MAX_MS                 10000U      // 时间上限：防止异常超长动作

#define FEETECH_STS_REACHED_MARGIN_DEG          3.0f        // 到位误差：当前角度与目标角度差值小于该值认为到位
#define FEETECH_STS_MOTION_SEND_PERIOD_MS       200U        // 保持发送周期：周期重发目标，降低偶发丢帧影响

#define FEETECH_STS_FEEDBACK_READ_ADDR          FEETECH_STS_ADDR_PRESENT_POSITION_L // 反馈读取起点：0x38 当前位置信息
#define FEETECH_STS_FEEDBACK_READ_LEN           15U                                 // 反馈读取长度：0x38~0x46，连续读取位置/速度/负载/电压/温度/移动/电流

#define FEETECH_STS_UPDATE_POSITION             0x01U       // 位标志：位置反馈更新
#define FEETECH_STS_UPDATE_TELEMETRY            0x02U       // 位标志：电压/温度/电流反馈更新
#define FEETECH_STS_UPDATE_STATUS               0x04U       // 位标志：在线/错误/运动状态更新
#define FEETECH_STS_UPDATE_RAW                  0x80U       // 位标志：原始应答帧已接收

/*============================================================
 * 5. 类型定义
 *============================================================*/

typedef enum {
    FEETECH_STS_CMD_NONE = 0,              // 命令类型：无待执行命令
    FEETECH_STS_CMD_PING,                  // 命令类型：查询舵机在线
    FEETECH_STS_CMD_READ,                  // 命令类型：通用读取寄存器
    FEETECH_STS_CMD_WRITE,                 // 命令类型：通用写入寄存器
    FEETECH_STS_CMD_REG_WRITE,             // 命令类型：异步写入寄存器，等待 ACTION 执行
    FEETECH_STS_CMD_ACTION,                // 命令类型：执行已缓存的异步写入
    FEETECH_STS_CMD_SET_TORQUE,            // 命令类型：设置扭矩使能
    FEETECH_STS_CMD_SET_MODE,              // 命令类型：设置舵机运行模式
    FEETECH_STS_CMD_SET_TARGET,            // 命令类型：写目标位置/时间/速度
    FEETECH_STS_CMD_SET_TARGET_EX,         // 命令类型：写加速度/目标位置/时间/速度
    FEETECH_STS_CMD_READ_FEEDBACK,         // 命令类型：读取综合反馈
    FEETECH_STS_CMD_SYNC_WRITE_TARGET,     // 命令类型：同步写多个舵机目标
    FEETECH_STS_CMD_RESET_STATUS           // 命令类型：重置总线状态缓存
} FeetechStsCommand_t;

typedef enum {
    FEETECH_STS_PACKET_NONE = 0,           // 应答类型：尚未收到有效包
    FEETECH_STS_PACKET_STATUS,             // 应答类型：普通状态包
    FEETECH_STS_PACKET_READ_DATA,          // 应答类型：READ 返回数据包
    FEETECH_STS_PACKET_ACK                 // 应答类型：写入/动作 ACK 状态包
} FeetechStsPacketType_t;

typedef struct {
    FeetechStsPacketType_t type;           // 应答分类：状态包/读数据包/ACK
    uint8_t id;                            // 应答 ID：返回包来源舵机 ID
    uint8_t error;                         // 应答错误码：飞特状态包 ERROR 字段
    uint8_t param_len;                     // 参数长度：params 中有效字节数
    uint8_t params[FEETECH_STS_PARAM_MAX_LEN]; // 参数缓存：READ 返回数据或状态参数
    uint8_t raw_len;                       // 原始帧长度：raw 中有效字节数
    uint8_t raw[FEETECH_STS_FRAME_MAX_LEN]; // 原始帧缓存：完整应答帧
    uint32_t tick_ms;                      // 接收时间戳：收到该应答的系统时间
    uint8_t unread;                        // 未读标志：1=最近原始应答尚未被上层取走
} FeetechStsPacket_t;

typedef struct {
    uint8_t id;                            // 反馈来源：当前反馈对应的舵机 ID
    uint8_t online;                        // 在线状态：最近收到有效应答
    uint8_t error;                         // 错误状态：舵机返回 ERROR 字段
    uint8_t moving;                        // 运动状态：1=正在运动，0=停止/到位

    uint16_t position;                     // 位置原始值：0~4095
    float angle_deg;                       // 位置角度：position -> 角度，单位度
    int16_t speed;                         // 当前速度：舵机反馈原始值
    int16_t load;                          // 当前负载：舵机反馈原始值
    uint8_t voltage_raw;                   // 电压原始值：常见单位 0.1V
    uint16_t voltage_mv;                   // 电压换算值：单位 mV
    uint8_t temperature_c;                 // 当前温度：单位摄氏度
    int16_t current_raw;                   // 当前电流：舵机反馈原始值
    uint16_t current_ma;                   // 电流换算值：单位 mA

    uint8_t position_valid;                // 有效标志：位置/角度反馈可用
    uint8_t telemetry_valid;               // 有效标志：电压/温度/电流反馈可用
    uint8_t update_flags;                  // 更新标志：位置/遥测/状态/原始帧更新位

    uint32_t last_update_tick;             // 最近更新时间：有效反馈更新时间戳
    uint32_t rx_packet_count;              // 接收计数：有效应答包数量
    uint32_t parse_error_count;            // 解析错误计数：校验/长度/格式错误
} FeetechStsFeedback_t;

typedef struct {
    uint8_t id;                            // 目标舵机 ID
    float target_angle_deg;                // 目标角度：单位度
    uint16_t target_position;              // 目标位置：0~4095
    uint16_t target_time_ms;               // 目标时间：单位 ms
    uint16_t target_speed;                 // 目标速度：0~4095，0=默认/内部策略
    uint8_t target_acc;                    // 目标加速度：写入扩展目标时使用
    uint8_t reached;                       // 到位状态：当前反馈是否接近目标
} FeetechStsMotion_t;

typedef struct {
    FeetechStsCommand_t command;           // 发送命令类型：最近一次发送的业务命令
    uint8_t id;                            // 发送 ID：最近一次目标舵机 ID
    uint8_t instruction;                   // 指令码：最近一次飞特协议指令
    uint8_t address;                       // 读写地址：最近一次寄存器地址
    uint8_t data_len;                      // 数据长度：最近一次读写参数长度
    uint8_t frame_len;                     // 发送帧长度：frame 中有效字节数
    uint8_t frame[FEETECH_STS_FRAME_MAX_LEN]; // 发送帧缓存：最近一次完整飞特帧
    uint32_t tx_count;                     // 发送计数：已发送命令帧数量
    uint32_t tick_ms;                      // 发送时间戳：最近一次发送时间
} FeetechStsTxState_t;

typedef struct {
    FeetechStsTxState_t tx;                // 发送状态：最近发送命令/帧/计数
    FeetechStsPacket_t packet;             // 应答缓存：最近一次有效应答包
    FeetechStsFeedback_t feedback;         // 反馈缓存：位置/电压/温度/电流/状态
    FeetechStsMotion_t motion;             // 运动目标：目标角度/位置/时间/速度
    HAL_StatusTypeDef last_status;         // HAL 状态：最近一次 UART/HAL 操作结果
} FeetechStsBus_t;

/*============================================================
 * 6. 外部接口
 *============================================================*/

uint32_t FeetechStsBus_RawRxBytes(void);
void FeetechStsBus_Init(void);                         // 初始化飞特总线：清状态 -> 启动接收 -> 准备收发。
HAL_StatusTypeDef FeetechStsBus_StartReceive(void);    // 启动接收：挂起 UART 接收中断。
void FeetechStsBus_HandleRxComplete(void);             // 接收完成处理：UART 字节 -> 组帧/解析 -> 更新反馈缓存。
uint8_t FeetechStsBus_TakeUpdateFlags(void);           // 取走更新标志：读取 update_flags -> 清零。

const FeetechStsBus_t *FeetechStsBus_GetBus(void);                 // 获取总线状态：返回总线只读快照。
const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void);        // 获取反馈缓存：位置/电压/温度/电流/状态。
const FeetechStsMotion_t *FeetechStsBus_GetMotion(void);            // 获取运动目标：目标角度/位置/时间/速度。
HAL_StatusTypeDef FeetechStsBus_GetLastStatus(void);                // 获取最近 HAL 状态：最近一次 UART/HAL 操作结果。
uint8_t FeetechStsBus_TakePacket(uint8_t *buffer, uint8_t buffer_size, uint8_t *out_len); // 取走原始应答：packet.raw -> 外部 buffer -> 清 unread。
HAL_StatusTypeDef FeetechStsBus_SendFrame(const uint8_t *frame, uint8_t len);             // 发送完整飞特帧：校验帧 -> UART 发送。
HAL_StatusTypeDef FeetechStsBus_SendFrameAndWaitResponse(const uint8_t *frame,
                                                         uint8_t len,
                                                         uint8_t *response,
                                                         uint8_t response_size,
                                                         uint8_t *response_len,
                                                         uint32_t timeout_ms);            // 发送并等待应答：发送帧 -> 等状态包/读包 -> 返回应答。

HAL_StatusTypeDef FeetechStsBus_Ping(uint8_t id);                                           // PING：查询指定 ID 是否在线。
HAL_StatusTypeDef FeetechStsBus_Read(uint8_t id, uint8_t address, uint8_t read_len);         // READ：读取寄存器 address 起始的 read_len 字节。
HAL_StatusTypeDef FeetechStsBus_Write(uint8_t id, uint8_t address, const uint8_t *data, uint8_t len); // WRITE：写寄存器数据。
HAL_StatusTypeDef FeetechStsBus_RegWrite(uint8_t id, uint8_t address, const uint8_t *data, uint8_t len); // REG_WRITE：异步写入寄存器，等待 ACTION 执行。
HAL_StatusTypeDef FeetechStsBus_Action(uint8_t id);                                         // ACTION：执行已缓存的异步写入。
HAL_StatusTypeDef FeetechStsBus_ResetStatus(uint8_t id);                                    // RESET_STATUS：清除/重置舵机状态。

HAL_StatusTypeDef FeetechStsBus_UnlockEeprom(uint8_t id);                                   // 解锁 EEPROM：允许修改 ID/波特率等参数。
HAL_StatusTypeDef FeetechStsBus_LockEeprom(uint8_t id);                                     // 锁定 EEPROM：保护参数不被误写。
HAL_StatusTypeDef FeetechStsBus_SetId(uint8_t current_id, uint8_t new_id);                   // 修改舵机 ID：current_id -> new_id。
HAL_StatusTypeDef FeetechStsBus_SetBaudRate(uint8_t id, uint8_t baud_index);                 // 设置波特率：写入飞特波特率索引。
HAL_StatusTypeDef FeetechStsBus_ReadPosition(uint8_t id);                                   // 读取位置：当前位置寄存器 -> feedback.position/angle。
HAL_StatusTypeDef FeetechStsBus_ReadVoltageTemperature(uint8_t id);                          // 读取电压温度：电压/温度寄存器 -> feedback。
HAL_StatusTypeDef FeetechStsBus_ReadCurrent(uint8_t id);                                    // 读取电流：当前电流寄存器 -> feedback.current。

HAL_StatusTypeDef FeetechStsBus_SetTorque(uint8_t id, uint8_t enable);                       // 设置扭矩使能：enable=1 上力，0 卸力。
HAL_StatusTypeDef FeetechStsBus_SetTorqueLimit(uint8_t id, uint16_t limit);                  // 设置扭矩限制：0~1000 原始限制值。
HAL_StatusTypeDef FeetechStsBus_SetTorqueLimitPercent(uint8_t id, uint8_t percent);          // 设置扭矩百分比：percent -> 原始扭矩限制。
HAL_StatusTypeDef FeetechStsBus_EnableTorque(uint8_t id);                                   // 使能扭矩：舵机上力保持。
HAL_StatusTypeDef FeetechStsBus_DisableTorque(uint8_t id);                                  // 关闭扭矩：舵机卸力释放。
HAL_StatusTypeDef FeetechStsBus_SetMode(uint8_t id, uint8_t mode);                           // 设置运行模式：位置/轮式等模式。
HAL_StatusTypeDef FeetechStsBus_SetPosition(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed); // 设置目标位置：位置/时间/速度 -> 舵机目标寄存器。
HAL_StatusTypeDef FeetechStsBus_SetAngle(uint8_t id, float angle_deg, uint16_t time_ms, uint16_t speed);      // 设置目标角度：角度 -> 位置计数 -> 发送目标。
HAL_StatusTypeDef FeetechStsBus_SetPositionEx(uint8_t id, uint16_t position, uint16_t time_ms, uint16_t speed, uint8_t acc); // 扩展目标位置：加速度/位置/时间/速度 -> 舵机。
HAL_StatusTypeDef FeetechStsBus_SetAngleEx(uint8_t id, float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc);      // 扩展目标角度：角度 -> 位置计数，加速度一并写入。
HAL_StatusTypeDef FeetechStsBus_ReadFeedback(uint8_t id);                                    // 读取综合反馈：位置/速度/负载/电压/温度/移动/电流。
HAL_StatusTypeDef FeetechStsBus_ReadFeedbackBlocking(uint8_t id, uint32_t timeout_ms);        // 阻塞读取反馈：发送 READ -> 等待反馈或超时。
HAL_StatusTypeDef FeetechStsBus_SyncWritePosition(uint8_t *ids, uint8_t id_count, uint16_t *positions, uint16_t *times_ms, uint16_t *speeds); // 同步写目标：多个 ID -> 同一帧批量写位置/时间/速度。

void FeetechStsMotion_SetTarget(float angle_deg, uint16_t time_ms, uint16_t speed);           // 设置运动目标：角度/时间/速度 -> motion 缓存。
void FeetechStsMotion_SetTargetEx(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc); // 设置扩展目标：角度/时间/速度/加速度 -> motion 缓存。
void FeetechStsMotion_Task(void);                                                            // 运动周期任务：motion 目标 -> 周期发送/到位判断。

uint16_t FeetechSts_AngleToPosition(float angle_deg);                  // 角度转位置：角度 -> 0~4095 位置计数。
float FeetechSts_PositionToAngle(uint16_t position);                   // 位置转角度：0~4095 位置计数 -> 角度。
uint16_t FeetechSts_LimitPosition(uint16_t position);                  // 限制位置：输入位置 -> 合法位置范围。
float FeetechSts_LimitAngle(float angle_deg);                          // 限制角度：输入角度 -> 项目允许角度范围。
uint16_t FeetechSts_MakeU16(uint8_t low, uint8_t high);                // 合成 16 位数：低字节/高字节 -> uint16_t。
void FeetechSts_SplitU16(uint8_t *low, uint8_t *high, uint16_t value);  // 拆分 16 位数：uint16_t -> 低字节/高字节。

uint8_t FeetechSts_BuildPacket(uint8_t id, uint8_t instruction, const uint8_t *params, uint8_t param_len, uint8_t *out, uint8_t out_max); // 组飞特帧：ID/指令/参数 -> 完整协议帧。
uint8_t FeetechSts_CalcChecksum(const uint8_t *packet, uint8_t total_len);       // 计算校验和：协议帧内容 -> CheckSum。
uint8_t FeetechSts_VerifyChecksum(const uint8_t *packet, uint8_t total_len);     // 校验协议帧：CheckSum 正确 -> 1。
uint8_t FeetechSts_GetFrameTotalLen(const uint8_t *packet, uint8_t current_len); // 计算整帧长度：已收头部/Len -> 总长度。
uint8_t FeetechSts_IsValidFrame(const uint8_t *packet, uint8_t total_len);       // 判断有效帧：帧头/长度/校验全部通过。

#ifdef __cplusplus
}
#endif

#endif
