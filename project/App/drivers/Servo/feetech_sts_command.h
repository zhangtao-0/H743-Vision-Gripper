/*********************************************************************************************************************
 * @file    feetech_sts_command.h
 * @brief   舵机命令层接口。
 *
 * @details
 * 1. 本文件负责：声明命令任务、转发入口和命令层状态。
 * 2. 主要流程：外部帧/按键 -> command 层统一调度。
 *********************************************************************************************************************/

#ifndef __FEETECH_STS_COMMAND_H__
#define __FEETECH_STS_COMMAND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "feature_config.h"
#include "feetech_sts_bus.h"
#include <stdint.h>

#define FEETECH_STS_CMD_SOURCE_NONE          0U      // 命令来源：无来源
#define FEETECH_STS_CMD_SOURCE_BLUETOOTH     1U      //         蓝牙 App，帧格式由蓝牙传输模式决定
#define FEETECH_STS_CMD_SOURCE_VOFA          2U      //         VOFA，接收裸官方帧
#define FEETECH_STS_CMD_SOURCE_KEY           3U      //         本地按键

#define FEETECH_STS_INST_LOCAL_STATUS_PRINT   0xF0U  // 本地指令，不转发舵机：控制/查询状态打印
#define FEETECH_STS_INST_LOCAL_EXAMPLE_PRINT  0xF1U  //                 打印常用官方协议
#define FEETECH_STS_INST_LOCAL_PARAM          0xF2U  //                 查询/修改本地参数
#define FEETECH_STS_INST_LOCAL_AUTHORITY      0xF3U  //                 旧控制权命令兼容，不改变运行状态
#define FEETECH_STS_INST_LOCAL_SAFETY         0xF4U  //                 机械极限管理

#define FEETECH_STS_COMMAND_RESP_TIMEOUT_MS   FEETECH_STS_RESP_TIMEOUT_MS // 应答超时：查询帧等待状态包时间
#define FEETECH_STS_COMMAND_FORWARD_WRITE_WAIT_RESP CLAW_FEATURE_STS_WRITE_ACK_WAIT_ENABLE // 非广播 WRITE/REG_WRITE 等待 ACK
#define FEETECH_STS_COMMAND_STATUS_TEXT_MAX   CLAW_STATUS_TEXT_CAPACITY    // 单条返回文本容量
#define FEETECH_STS_COMMAND_ECHO_RX_FRAME     CLAW_FEATURE_RX_FRAME_ECHO_ENABLE // 收帧回显开关
#define FEETECH_STS_COMMAND_VOFA_TELEMETRY_ENABLE  CLAW_FEATURE_VOFA_TELEMETRY_SUPPORT // 编译 claw 反馈
#define FEETECH_STS_COMMAND_VOFA_TELEMETRY_MS      CLAW_DEFAULT_AUTO_PRINT_PERIOD_MS // 默认反馈周期
  
typedef struct {
    uint32_t bluetooth_rx_count;       // 入口统计：蓝牙收到有效官方帧次数
    uint32_t vofa_rx_count;            // 入口统计：VOFA 收到有效官方帧次数
    uint32_t key_event_count;          // 入口统计：本地按键动作触发次数

    uint32_t forward_ok_count;         // 转发统计：官方帧转发成功次数
    uint32_t forward_error_count;      // 转发统计：官方帧转发失败次数
    uint32_t response_count;           // 转发统计：已回传舵机应答次数
    uint32_t passthrough_count;        // 转发统计：WRITE/ACTION 类帧直接透传次数
    uint32_t query_count;              // 转发统计：PING/READ 等查询帧次数

    uint8_t last_source;               // 最近命令：来源，蓝牙/VOFA/按键
    uint8_t last_frame_len;            // 最近命令：转发帧长度
    uint8_t last_instruction;          // 最近命令：飞特指令码
    HAL_StatusTypeDef last_status;     // 最近命令：转发/动作执行结果
} FeetechStsCommandStatus_t;

void FeetechStsCommand_Init(void);     // 初始化命令层。
void FeetechStsCommand_Task(void);     // 周期检查：按键/蓝牙/VOFA -> 控制层。
HAL_StatusTypeDef FeetechStsCommand_ForwardFrame(const uint8_t *frame, uint8_t len, uint8_t source); // 外部帧 -> 本地命令/总线转发。
const FeetechStsCommandStatus_t *FeetechStsCommand_GetStatus(void); // 获取命令层统计状态。

#ifdef __cplusplus
}
#endif

#endif
