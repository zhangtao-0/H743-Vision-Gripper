/*********************************************************************************************************************
 * @file    feetech_sts_command.c
 * @brief   舵机命令入口与按键调度实现。
 *
 * @details
 * 1. 本文件负责：分发蓝牙、VOFA、本地按键和参数命令。
 * 2. 本文件不负责：实现底层 UART 组帧。
 * 3. 主要流程：输入源 -> 命令解析 -> 控制层/总线层。
 *********************************************************************************************************************/

#include "feetech_sts_command.h"
#include "bsp_bluetooth.h"
#include "bsp_key.h"
#include "claw_host_protocol.h"
#include "feetech_sts_control.h"
#include "feetech_sts_register.h"
#include "pwm_servo_control.h"
#include "vofa_protocol.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define FEETECH_STS_COMMAND_A0_DOUBLE_CLICK_MS      FEETECH_STS_CONTROL_A0_DOUBLE_CLICK_MS // A0 单击/双击确认窗口
#define FEETECH_STS_COMMAND_B0_MULTI_CLICK_MS       350U                                 // B0 多击确认窗口
#define FEETECH_STS_COMMAND_LONG_PRESS_MS           KEY_LONG_PRESS_TIME_MS               // A0/B0 长按判定
#define FEETECH_STS_COMMAND_SWEEP_PROGRESS_MS       100U  // B0扫描位置打印周期。

/*============================================================
 * 1. 内部变量
 *============================================================*/

typedef struct {
    uint8_t valid;
    float angle_deg;
    uint16_t time_ms;
    uint16_t speed;
    uint8_t acc;
} FeetechStsCommandTarget_t;

typedef enum {
    FEETECH_STS_BT_SLIDER_SCALE_MOTION_RANGE = 0,
    FEETECH_STS_BT_SLIDER_SCALE_FIXED_RANGE
} FeetechStsBluetoothSliderScale_t;

typedef struct {
    uint8_t channel;
    uint8_t host_command;
    FeetechStsBluetoothSliderScale_t scale;
    float min_value;
    float max_value;
} FeetechStsBluetoothSliderRoute_t;

static const FeetechStsBluetoothSliderRoute_t s_bluetooth_slider_routes[] = {
    { BLUETOOTH_SLIDER_CHANNEL_ANGLE, CLAW_HOST_CMD_ANGLE,
      FEETECH_STS_BT_SLIDER_SCALE_MOTION_RANGE, 0.0f, 0.0f }
};

static FeetechStsCommandStatus_t s_sts_command;        // 命令层状态统计
static FeetechStsCommandTarget_t s_pending_reg_target; // REG_WRITE 暂存目标
static uint8_t s_a0_stop_click_pending;                // A0 按下已执行急停
static uint8_t s_mode_print_pending;                   // 下次运动打印行程模式
static uint32_t s_vofa_telemetry_tick_ms;              // 四项反馈节拍
static uint8_t s_telemetry_source;                     // 周期反馈返回入口
static uint32_t s_sweep_progress_print_tick_ms;     // B0过程打印节拍。
static uint32_t s_sweep_progress_feedback_tick_ms;  // 已打印反馈时间戳。

static KeyGesture_t s_a0_gesture;                      // A0 唯一手势识别器
static KeyGesture_t s_b0_gesture;                      // B0 唯一手势识别器
static uint8_t s_vofa_a0_pressed;                      // VOFA A0 电平
static uint8_t s_vofa_b0_pressed;                      // VOFA B0 电平
static uint8_t s_bluetooth_a0_pressed;                 // 蓝牙 A0 电平
static uint8_t s_bluetooth_b0_pressed;                 // 蓝牙 B0 电平
static uint32_t s_host_a_deadline,s_host_b_deadline;
static uint8_t s_key_a0_pressed;                       // 实体 A0 消抖电平
static uint8_t s_key_b0_pressed;                       // 实体 B0 消抖电平
static uint8_t s_effective_a0_pressed;                 // A有效=实体 OR VOFA OR 蓝牙
static uint8_t s_effective_b0_pressed;                 // B有效=实体 OR VOFA OR 蓝牙
static uint8_t s_a0_event_source;                      // A0 最近有效来源
static uint8_t s_b0_event_source;                      // B0 最近有效来源

static uint8_t s_combo_candidate;                     // A+B 同时按住计时中
static uint8_t s_combo_latched;                       // A+B 已触发，等待全部释放
static uint32_t s_combo_start_tick_ms;                // A+B 起始时间
static uint8_t s_a0_only_tracking;                    // A 单独长按计时中
static uint32_t s_a0_only_start_tick_ms;              // A 单独长按起始时间
static uint8_t s_range_select_active;                 // 行程模式选择中
static FeetechStsSweepRange_t s_range_candidate;      // 当前候选行程

/*============================================================
 * 2. 内部函数声明
 *============================================================*/

// 2.1 输入入口调度
static void FeetechStsCommand_TaskBluetooth(void);
static void FeetechStsCommand_HandleBluetoothSlider(const BluetoothSliderEvent_t *event);
static const FeetechStsBluetoothSliderRoute_t *FeetechStsCommand_FindBluetoothSliderRoute(uint8_t channel);
static void FeetechStsCommand_TaskVofa(void);
static void FeetechStsCommand_TaskKey(void);
static void FeetechStsCommand_TaskHostKeys(uint32_t now);

// 2.2 统一动作和实体按键映射
static void FeetechStsCommand_HandleA0KeyEvents(uint16_t events, uint8_t source);
static void FeetechStsCommand_HandleB0KeyEvents(uint16_t events, uint8_t source);
static void FeetechStsCommand_SetHostKeyLevel(uint8_t command, uint8_t pressed, uint8_t source);
static HAL_StatusTypeDef FeetechStsCommand_ExecuteAction(uint8_t command, int32_t action, uint8_t source);
static HAL_StatusTypeDef FeetechStsCommand_HandleHostCommand(const uint8_t *frame, uint8_t len, uint8_t source);
static FeetechStsControlOwner_t FeetechStsCommand_ActionOwner(uint8_t command, uint8_t source);
static void FeetechStsCommand_PrintModeOnNextMotion(uint8_t source);
static void FeetechStsCommand_ResetUnifiedGestures(uint32_t now);
static uint8_t FeetechStsCommand_CombinedSource(void);

// 2.3 官方帧转发和控制权
static uint8_t FeetechStsCommand_HandleLocalFrame(const uint8_t *frame, uint8_t len, uint8_t source);
static uint8_t FeetechStsCommand_IsWriteLikeFrame(const uint8_t *frame, uint8_t len);
static uint8_t FeetechStsCommand_IsControlFrame(const uint8_t *frame, uint8_t len);
static uint8_t FeetechStsCommand_WriteTouchesTarget(uint8_t address, uint8_t data_len);
static void FeetechStsCommand_SyncExternalTarget(const uint8_t *frame, uint8_t len, uint8_t source);
static uint8_t FeetechStsCommand_ParseTargetFrame(const uint8_t *frame, uint8_t len, FeetechStsCommandTarget_t *out_target);
static uint8_t FeetechStsCommand_ParseWriteTarget(uint8_t address, const uint8_t *data, uint8_t data_len, FeetechStsCommandTarget_t *out_target);
static uint8_t FeetechStsCommand_ShouldWaitResponse(const uint8_t *frame, uint8_t len);
static uint8_t FeetechStsCommand_ValidateWriteFrame(const uint8_t *frame, uint8_t len, uint8_t source);
static void FeetechStsCommand_PrintWriteReject(uint8_t source, uint8_t address, FeetechStsRegisterCheckResult_t result, const FeetechStsRegisterDesc_t *desc, int32_t value);
static FeetechStsControlOwner_t FeetechStsCommand_SourceToOwner(uint8_t source);
static uint8_t FeetechStsCommand_ActionOutputSource(void);

// 2.4 本地扩展命令处理
static void FeetechStsCommand_HandleParamCommand(const uint8_t *params, uint8_t param_len, uint8_t source);
static void FeetechStsCommand_HandleAuthorityCommand(const uint8_t *params, uint8_t param_len, uint8_t source);
static void FeetechStsCommand_HandleSafetyCommand(const uint8_t *params, uint8_t param_len, uint8_t source);
static void FeetechStsCommand_SendSafetyAck(uint8_t source, uint8_t action, HAL_StatusTypeDef status);
static void FeetechStsCommand_SendExampleText(uint8_t source);
static void FeetechStsCommand_SendParamList(uint8_t source);
static void FeetechStsCommand_SendParamInfo(uint8_t source, FeetechStsParamId_t id);

// 2.5 文本和状态打印
static void FeetechStsCommand_SendText(uint8_t source, const char *text);
static void FeetechStsCommand_Printf(uint8_t source, const char *fmt, ...);
static void FeetechStsCommand_SendTelemetryOnce(uint8_t source);
static void FeetechStsCommand_SendCommandSentText(const uint8_t *frame, uint8_t len, uint8_t source, HAL_StatusTypeDef status);
static void FeetechStsCommand_SendCommandResponseText(const uint8_t *frame, uint8_t frame_len, const uint8_t *response, uint8_t response_len, uint8_t source, HAL_StatusTypeDef status);
static uint8_t FeetechStsCommand_PrintReadResponse(uint8_t address, uint8_t read_len, const uint8_t *response, uint8_t response_len, uint8_t source);
static void FeetechStsCommand_PrintReadRaw(uint8_t address, const uint8_t *data, uint8_t data_len, uint8_t source);
static void FeetechStsCommand_PrintRegisterRead(uint8_t source, const FeetechStsRegisterDesc_t *desc, const uint8_t *data);
static void FeetechStsCommand_PrintWriteSummary(const uint8_t *frame,uint8_t frame_len,uint8_t source,const char *prefix);
static void FeetechStsCommand_PrintServoStatus(uint8_t source, const char *prefix, uint8_t error);
static void FeetechStsCommand_AppendStatusItem(char *text, uint16_t text_size, const char *item);
static void FeetechStsCommand_TaskManagerPrint(uint32_t now);
static void FeetechStsCommand_TaskGripDetectPrint(void);
static void FeetechStsCommand_TaskSweepPrint(void);
static void FeetechStsCommand_TaskSweepProgressPrint(uint32_t now);
static void FeetechStsCommand_TaskMotionPrint(void);

#if (FEETECH_STS_COMMAND_ECHO_RX_FRAME != 0U)
static void FeetechStsCommand_SendResponse(const uint8_t *frame, uint8_t len, uint8_t source);
#endif

// 2.6 名称转换
static const char *FeetechStsCommand_InstructionNameCn(uint8_t instruction);
static const char *FeetechStsCommand_ParamNameCn(FeetechStsParamId_t id);
static const char *FeetechStsCommand_HalNameCn(HAL_StatusTypeDef status);
static void FeetechStsCommand_PrintCurrentTarget(uint8_t source, const char *text);

/*============================================================
 * 3. 初始化和周期任务
 *============================================================*/

// 初始化命令层。
void FeetechStsCommand_Init(void)
{
    memset(&s_sts_command, 0, sizeof(s_sts_command));
    memset(&s_pending_reg_target, 0, sizeof(s_pending_reg_target));
    s_sts_command.last_status = HAL_OK;
    s_sts_command.last_source = FEETECH_STS_CMD_SOURCE_NONE;
    s_a0_stop_click_pending = 0U;
    s_mode_print_pending = 0U;
    s_vofa_telemetry_tick_ms = 0UL;
    s_telemetry_source = FEETECH_STS_CMD_SOURCE_VOFA;
    s_vofa_a0_pressed = 0U;
    s_vofa_b0_pressed = 0U;
    s_bluetooth_a0_pressed = 0U;
    s_bluetooth_b0_pressed = 0U;
    s_key_a0_pressed = Key_GetFlag(Key_Task5);
    s_key_b0_pressed = Key_GetFlag(Key_Task6);
    s_effective_a0_pressed = s_key_a0_pressed;
    s_effective_b0_pressed = s_key_b0_pressed;
    s_a0_event_source = FEETECH_STS_CMD_SOURCE_KEY;
    s_b0_event_source = FEETECH_STS_CMD_SOURCE_KEY;
    s_combo_candidate = 0U;
    s_combo_latched = 0U;
    s_combo_start_tick_ms = 0UL;
    s_a0_only_tracking = 0U;
    s_a0_only_start_tick_ms = 0UL;
    s_range_select_active = 0U;
    
    s_sweep_progress_print_tick_ms = 0UL;
    s_sweep_progress_feedback_tick_ms = 0UL;
    
    s_range_candidate = FeetechStsControl_GetSweepRange();
    FeetechStsCommand_ResetUnifiedGestures(HAL_GetTick());

    FeetechStsCommand_SendText(FEETECH_STS_CMD_SOURCE_NONE,
        "复位成功：默认范围=0~270度，未主动下发位置目标");
    FeetechStsCommand_SendText(FEETECH_STS_CMD_SOURCE_NONE,
        "标定方法：A+B长按3秒进入/保存；标定中B按住扫描，A单击闭合端，A双击张开端");
    FeetechStsCommand_SendText(FEETECH_STS_CMD_SOURCE_NONE,
        "范围选择：A单独长按3秒进入；B切换机械极限/0~270度；A确认");
}

// 命令层任务：按键、蓝牙、VOFA、控制层、状态打印。
void FeetechStsCommand_Task(void)
{
    uint32_t now;

    now = HAL_GetTick();

    FeetechStsCommand_TaskKey();
    FeetechStsCommand_TaskBluetooth();
    FeetechStsCommand_TaskVofa();
    FeetechStsCommand_TaskHostKeys(now);

    FeetechStsControl_Task();

    now = HAL_GetTick();

    FeetechStsCommand_TaskGripDetectPrint();
    FeetechStsCommand_TaskSweepPrint();
    FeetechStsCommand_TaskSweepProgressPrint(now);
    FeetechStsCommand_TaskMotionPrint();
    FeetechStsCommand_TaskManagerPrint(now);
}

/*============================================================
 * 4. 官方帧转发
 *============================================================*/

// 协议路由：收到一帧 -> 判断业务类型 -> 选择转发路径 -> 返回结果。
HAL_StatusTypeDef FeetechStsCommand_ForwardFrame(const uint8_t *frame, uint8_t len, uint8_t source)
{
    uint8_t response[FEETECH_STS_FRAME_MAX_LEN];    // 舵机应答缓存
    uint8_t response_len;                           // 舵机应答长度
    uint8_t instruction;                            // 当前帧指令码
    HAL_StatusTypeDef status;                       // 转发/等待结果

    response_len = 0U;

    if ((FeetechSts_IsValidFrame(frame, len) == 0U) &&
        (ClawHostProtocol_IsFrame(frame, len) == 0U)) { // 无效官方帧/F5 帧 -> 拒绝
        s_sts_command.last_status = HAL_ERROR;
        s_sts_command.forward_error_count++;
        FeetechStsCommand_Printf(source, "无效协议帧");
        return HAL_ERROR;
    }

    instruction = frame[4];
    s_sts_command.last_source = source;             // 记录最近命令来源
    s_sts_command.last_frame_len = len;             // 记录最近帧长度
    s_sts_command.last_instruction = instruction;   // 记录最近指令码

    if (FeetechStsCommand_HandleLocalFrame(frame, len, source) != 0U) { //本地指令判断
        if ((instruction != FEETECH_STS_INST_LOCAL_SAFETY) &&
            (instruction != CLAW_HOST_INST_ACTION)) {
            s_sts_command.last_status = HAL_OK;
        }
        return s_sts_command.last_status;
    }

#if (FEETECH_STS_COMMAND_ECHO_RX_FRAME != 0U)
    FeetechStsCommand_SendResponse(frame, len, source); //回显收到的完整官方帧
#endif
    if (FeetechStsCommand_ValidateWriteFrame(frame, len, source) == 0U) {
        s_sts_command.last_status = HAL_ERROR;
        s_sts_command.forward_error_count++;
        return HAL_ERROR;
    }

    if (FeetechStsCommand_IsControlFrame(frame, len) != 0U) {           // 控制帧 -> 新来源覆盖旧来源
        FeetechStsCommand_PrintModeOnNextMotion(source);
        (void)FeetechStsControl_ExternalWriteBegin(FeetechStsCommand_SourceToOwner(source));
        FeetechStsCommand_SyncExternalTarget(frame, len, source);
    }

    if ((FeetechStsCommand_IsWriteLikeFrame(frame, len) != 0U) &&
        (FEETECH_STS_COMMAND_FORWARD_WRITE_WAIT_RESP == 0U)) {  // WRITE 类帧默认直发，不等待状态包
        status = FeetechStsBus_SendFrame(frame, len);
        s_sts_command.last_status = status;
        FeetechStsControl_SyncManager();
        FeetechStsCommand_SendCommandSentText(frame, len, source, status);

        if (status == HAL_OK) {
            s_sts_command.forward_ok_count++;
            s_sts_command.passthrough_count++;
        } else {
            s_sts_command.forward_error_count++;
        }

        return status;
    }

    if (FeetechStsCommand_ShouldWaitResponse(frame, len) != 0U) { // 查询帧 -> 发送并等待舵机应答
        s_sts_command.query_count++;
        status = FeetechStsBus_SendFrameAndWaitResponse(frame,
                                                        len,
                                                        response,
                                                        sizeof(response),
                                                        &response_len,
                                                        FEETECH_STS_COMMAND_RESP_TIMEOUT_MS);
        s_sts_command.last_status = status;
        FeetechStsControl_SyncManager();
        FeetechStsCommand_SendCommandResponseText(frame, len, response, response_len, source, status);

        if ((status == HAL_OK) || (status == HAL_TIMEOUT)) {
            s_sts_command.forward_ok_count++;
        } else {
            s_sts_command.forward_error_count++;
        }

        return status;
    }

    status = FeetechStsBus_SendFrame(frame, len);    // 其他帧 -> 直接转发
    s_sts_command.last_status = status;
    FeetechStsControl_SyncManager();
    FeetechStsCommand_SendCommandSentText(frame, len, source, status);

    if (status == HAL_OK) {
        s_sts_command.forward_ok_count++;
        s_sts_command.passthrough_count++;
    } else {
        s_sts_command.forward_error_count++;
    }

    return status;
}

// 获取命令层状态：返回统计计数/最近命令结果。
const FeetechStsCommandStatus_t *FeetechStsCommand_GetStatus(void)
{
    return &s_sts_command;
}

/*============================================================
 * 5. 各入口任务
 *============================================================*/

// 蓝牙任务：裸帧直接转发；滑块整数先映射为统一 F5 帧。
static void FeetechStsCommand_TaskBluetooth(void)
{
    uint8_t frame[FEETECH_STS_FRAME_MAX_LEN];
    uint8_t frame_len;
    BluetoothSliderEvent_t slider_event;

    frame_len = 0U;
    if (Bluetooth_ReadFeetechFrame(frame, sizeof(frame), &frame_len) != 0U) {
        s_sts_command.bluetooth_rx_count++;
        (void)FeetechStsCommand_ForwardFrame(frame, frame_len, FEETECH_STS_CMD_SOURCE_BLUETOOTH);
    }
    if (Bluetooth_ReadSliderEvent(&slider_event) != 0U) {
        s_sts_command.bluetooth_rx_count++;
        FeetechStsCommand_HandleBluetoothSlider(&slider_event);
    }
}

// 滑块通道和值 -> 既有 F5 命令；后续滑块只扩展路由表和映射类型。
static void FeetechStsCommand_HandleBluetoothSlider(const BluetoothSliderEvent_t *event)
{
    const FeetechStsBluetoothSliderRoute_t *route;
    uint8_t frame[CLAW_HOST_FRAME_TOTAL_LEN];
    uint8_t frame_len;
    float value;
    float min_angle;
    float max_angle;

    if ((event == 0) || (event->percent > BLUETOOTH_SLIDER_VALUE_MAX)) {
        return;
    }
    route = FeetechStsCommand_FindBluetoothSliderRoute(event->channel);
    if (route == 0) {
        FeetechStsCommand_Printf(FEETECH_STS_CMD_SOURCE_BLUETOOTH,
                                 "未知蓝牙滑块通道 %u",
                                 event->channel);
        return;
    }

    value = 0.0f;
    if (route->scale == FEETECH_STS_BT_SLIDER_SCALE_MOTION_RANGE) {
        min_angle = FeetechStsControl_GetMotionMinAngle();
        max_angle = FeetechStsControl_GetMotionMaxAngle();
        value = min_angle + (max_angle - min_angle) * ((float)event->percent / 100.0f);
    } else {
        value = route->min_value +
                (route->max_value - route->min_value) * ((float)event->percent / 100.0f);
    }

    frame_len = ClawHostProtocol_Build(route->host_command, value, frame, sizeof(frame));
    if (frame_len != 0U) {
        (void)FeetechStsCommand_ForwardFrame(frame,
                                             frame_len,
                                             FEETECH_STS_CMD_SOURCE_BLUETOOTH);
    }
}

// 按通道查找滑块路由。
static const FeetechStsBluetoothSliderRoute_t *FeetechStsCommand_FindBluetoothSliderRoute(uint8_t channel)
{
    uint8_t i;

    for (i = 0U;
         i < (uint8_t)(sizeof(s_bluetooth_slider_routes) / sizeof(s_bluetooth_slider_routes[0]));
         i++) {
        if (s_bluetooth_slider_routes[i].channel == channel) {
            return &s_bluetooth_slider_routes[i];
        }
    }
    return 0;
}

// VOFA 任务：读取帧并转发。
static void FeetechStsCommand_TaskVofa(void)
{
    uint8_t frame[FEETECH_STS_FRAME_MAX_LEN];
    uint8_t frame_len;

    frame_len = 0U;
    if (VOFA_Protocol_ReadFeetechFrame(frame, sizeof(frame), &frame_len) != 0U) {
        s_sts_command.vofa_rx_count++;
        (void)FeetechStsCommand_ForwardFrame(frame, frame_len, FEETECH_STS_CMD_SOURCE_VOFA);
    }
}

// 实体按键任务：读取消抖电平，交给统一手势识别器。
static void FeetechStsCommand_TaskKey(void)
{
    uint8_t pressed;

    pressed = Key_GetFlag(Key_Task5);
    if (pressed != s_key_a0_pressed) {
        s_key_a0_pressed = pressed;
        s_a0_event_source = FEETECH_STS_CMD_SOURCE_KEY;
        s_sts_command.key_event_count++;
    }
    pressed = Key_GetFlag(Key_Task6);
    if (pressed != s_key_b0_pressed) {
        s_key_b0_pressed = pressed;
        s_b0_event_source = FEETECH_STS_CMD_SOURCE_KEY;
        s_sts_command.key_event_count++;
    }
}

// 统一按键任务：多来源电平 -> A/B 手势 -> 组合键/业务动作。
static void FeetechStsCommand_TaskHostKeys(uint32_t now)
{
    uint8_t effective_a0;
    uint8_t effective_b0;
    uint16_t a0_events;
    uint16_t b0_events;
    HAL_StatusTypeDef status;
    uint8_t source;

    if((int32_t)(now-s_host_a_deadline)>=0)s_vofa_a0_pressed=s_bluetooth_a0_pressed=0;
    if((int32_t)(now-s_host_b_deadline)>=0)s_vofa_b0_pressed=s_bluetooth_b0_pressed=0;
    Input_LegacyHeld(s_vofa_a0_pressed||s_vofa_b0_pressed||s_bluetooth_a0_pressed||s_bluetooth_b0_pressed);
    if(Input_GetMode()==INPUT_PHYSICAL){s_vofa_a0_pressed=s_vofa_b0_pressed=0;s_bluetooth_a0_pressed=s_bluetooth_b0_pressed=0;}
    effective_a0 = ((s_key_a0_pressed != 0U) || (s_vofa_a0_pressed != 0U) ||
                    (s_bluetooth_a0_pressed != 0U)) ? 1U : 0U;
    effective_b0 = ((s_key_b0_pressed != 0U) || (s_vofa_b0_pressed != 0U) ||
                    (s_bluetooth_b0_pressed != 0U)) ? 1U : 0U;
    if(!Input_DebugEnabled(0)){
        s_effective_a0_pressed=s_effective_b0_pressed=0;
        KeyGesture_Init(&s_a0_gesture);KeyGesture_Init(&s_b0_gesture);
        s_a0_stop_click_pending=s_combo_candidate=s_combo_latched=s_a0_only_tracking=0;
        return;
    }
    if (effective_a0 != s_effective_a0_pressed) {
        s_effective_a0_pressed = effective_a0;
        KeyGesture_SetPressed(&s_a0_gesture, effective_a0, now);
    }
    if (effective_b0 != s_effective_b0_pressed) {
        s_effective_b0_pressed = effective_b0;
        KeyGesture_SetPressed(&s_b0_gesture, effective_b0, now);
    }

    if ((effective_a0 != 0U) && (effective_b0 != 0U)) {
        s_a0_only_tracking = 0U;
        if (s_combo_candidate == 0U) {
            s_combo_candidate = 1U;
            s_combo_start_tick_ms = now;        // 组合键立即停止旧运动
            if (FeetechStsControl_IsMotionState() != 0U) {
                (void)FeetechStsControl_StopMotion();
            }
        }
        if ((s_combo_latched == 0U) &&
            ((uint32_t)(now - s_combo_start_tick_ms) >= g_claw_feature_defaults.limit_combo_hold_ms)) {
            source = FeetechStsCommand_CombinedSource();
            if (FeetechStsControl_IsMotionState() != 0U) {
                (void)FeetechStsControl_StopMotion();
            }
            if (FeetechStsControl_IsLimitSetting() != 0U) {
                status = FeetechStsControl_FinishLimitSetting();
                FeetechStsCommand_SendText(source,
                    (status == HAL_OK) ? "机械极限已校验并保存" : "机械极限保存失败：请确认两端已记录且间距有效");
            } else {
                status = FeetechStsControl_BeginLimitSetting();
                FeetechStsCommand_SendText(source,
                    (status == HAL_OK) ? "进入极限标定：B长按扫描，A单击夹取端，双击张开端" : "进入极限标定失败");
            }
            s_combo_latched = 1U;
            s_range_select_active = 0U;
        }
        (void)KeyGesture_Task(&s_a0_gesture, now);
        (void)KeyGesture_Task(&s_b0_gesture, now);
        return;
    }

    if (s_combo_candidate != 0U) {
        s_combo_candidate = 0U;
        if (s_combo_latched != 0U) {
            if ((effective_a0 == 0U) && (effective_b0 == 0U)) {
                s_combo_latched = 0U;
                FeetechStsCommand_ResetUnifiedGestures(now);
            }
            return;
        }
        FeetechStsCommand_ResetUnifiedGestures(now);
    }
    if (s_combo_latched != 0U) {
        if ((effective_a0 == 0U) && (effective_b0 == 0U)) {
            s_combo_latched = 0U;
            FeetechStsCommand_ResetUnifiedGestures(now);
        }
        return;
    }

    if ((effective_a0 != 0U) && (effective_b0 == 0U) &&
        (s_range_select_active == 0U) && (FeetechStsControl_IsLimitSetting() == 0U)) {
        if (s_a0_only_tracking == 0U) {
            s_a0_only_tracking = 1U;
            s_a0_only_start_tick_ms = now;
        } else if ((uint32_t)(now - s_a0_only_start_tick_ms) >=
                   g_claw_feature_defaults.range_select_hold_ms) {
            s_a0_only_tracking = 0U;
            s_range_select_active = 1U;
            s_range_candidate = FeetechStsControl_GetSweepRange();
            s_a0_gesture.long_reported = 1U;
            FeetechStsCommand_SendText(s_a0_event_source,
                (s_range_candidate == FEETECH_STS_SWEEP_RANGE_LIMITS) ?
                "行程选择：机械极限；B单击切换，A单击确认" :
                "行程选择：0~270度；B单击切换，A单击确认");
        }
    } else {
        s_a0_only_tracking = 0U;
    }

    a0_events = KeyGesture_Task(&s_a0_gesture, now);
    b0_events = KeyGesture_Task(&s_b0_gesture, now);
    if (a0_events != KEY_EVENT_NONE) {
        FeetechStsCommand_HandleA0KeyEvents(a0_events, s_a0_event_source);
    }
    if (b0_events != KEY_EVENT_NONE) {
        FeetechStsCommand_HandleB0KeyEvents(b0_events, s_b0_event_source);
    }
}

// A0 事件：普通模式夹取/张开/急停；标定模式记录端点。
static void FeetechStsCommand_HandleA0KeyEvents(uint16_t events, uint8_t source)
{
    HAL_StatusTypeDef status;
    const FeetechStsControlStatus_t *control;

    if ((events & KEY_EVENT_PRESS) != 0U) {
        s_a0_stop_click_pending = 0U;
        if (FeetechStsControl_IsMotionState() != 0U) {
            if (FeetechStsControl_IsLimitSetting() == 0U) {
                s_a0_stop_click_pending = 1U;
            }
            status = FeetechStsControl_StopMotion();
            if (status == HAL_OK) {
                FeetechStsControl_SetActionSource(FeetechStsCommand_ActionOwner(CLAW_HOST_CMD_A0, source));
                FeetechStsCommand_PrintCurrentTarget(source, "A0急停");
            } else {
                FeetechStsCommand_SendText(source, "A0急停失败：位置反馈无效");
            }
        }
    }

    if (s_range_select_active != 0U) {
        if ((events & KEY_EVENT_SINGLE_CLICK) != 0U) {
            status = FeetechStsControl_SaveSweepRange(s_range_candidate);
            if (status == HAL_OK) {
                s_range_select_active = 0U;
                s_mode_print_pending = 1U;
                FeetechStsCommand_SendText(source,
                    (s_range_candidate == FEETECH_STS_SWEEP_RANGE_LIMITS) ?
                    "行程已保存：机械极限" : "行程已保存：0~270度");
            } else {
                FeetechStsCommand_SendText(source, "行程保存失败：请先设置机械极限");
            }
        }
        return;
    }

    if (FeetechStsControl_IsLimitSetting() != 0U) {
        if ((events & KEY_EVENT_DOUBLE_CLICK) != 0U) {
            status = FeetechStsControl_RecordOpenLimit();
            control = FeetechStsControl_GetStatus();
            if ((status == HAL_OK) && (control != 0)) {
                FeetechStsCommand_Printf(source, "张开端已记录 编码=%u",
                                         control->pending_open_position);
            } else {
                FeetechStsCommand_SendText(source, "反馈无效，张开端未记录");
            }
        } else if ((events & KEY_EVENT_SINGLE_CLICK) != 0U) {
            status = FeetechStsControl_RecordCloseLimit();
            control = FeetechStsControl_GetStatus();
            if ((status == HAL_OK) && (control != 0)) {
                FeetechStsCommand_Printf(source, "夹取端已记录 编码=%u",
                                         control->pending_close_position);
            } else {
                FeetechStsCommand_SendText(source, "反馈无效，夹取端未记录");
            }
        }
        return;
    }

    if ((events & KEY_EVENT_DOUBLE_CLICK) != 0U) {
        s_a0_stop_click_pending = 0U;
        (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_A0, 2, source);
    } else if ((events & KEY_EVENT_SINGLE_CLICK) != 0U) {
        if (s_a0_stop_click_pending != 0U) {
            s_a0_stop_click_pending = 0U;
        } else {
            (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_A0, 1, source);
        }
    }
}

// B0 事件：扫描/停止/换向；行程选择时切换候选。
static void FeetechStsCommand_HandleB0KeyEvents(uint16_t events, uint8_t source)
{
    if (s_range_select_active != 0U) {
        if ((events & KEY_EVENT_SINGLE_CLICK) != 0U) {
            s_range_candidate = (s_range_candidate == FEETECH_STS_SWEEP_RANGE_LIMITS) ?
                                FEETECH_STS_SWEEP_RANGE_FULL_270 : FEETECH_STS_SWEEP_RANGE_LIMITS;
            FeetechStsCommand_SendText(source,
                (s_range_candidate == FEETECH_STS_SWEEP_RANGE_LIMITS) ?
                "行程候选：机械极限" : "行程候选：0~270度");
        }
        return;
    }

    if (FeetechStsControl_IsLimitSetting() != 0U) {
        if ((events & KEY_EVENT_LONG_START) != 0U) {
            (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 1, source);
        }
        if ((events & KEY_EVENT_RELEASE) != 0U) {
            (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 0, source);
        }
        if ((events & KEY_EVENT_DOUBLE_CLICK) != 0U) {
            (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 2, source);
        }
        return;
    }

    if ((events & KEY_EVENT_PRESS) != 0U) {
        (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 1, source);
    }
    if ((events & KEY_EVENT_RELEASE) != 0U) {
        (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 0, source);
    }
    if ((events & KEY_EVENT_DOUBLE_CLICK) != 0U) {
        (void)FeetechStsCommand_ExecuteAction(CLAW_HOST_CMD_B0, 2, source);
    }
}

// 执行统一动作：VOFA、蓝牙和实体按键共用。
static HAL_StatusTypeDef FeetechStsCommand_ExecuteAction(uint8_t command, int32_t action, uint8_t source)
{
    HAL_StatusTypeDef status;
    FeetechStsControlOwner_t owner;

    status = HAL_ERROR;
    owner = FeetechStsCommand_ActionOwner(command, source);

    if (command == CLAW_HOST_CMD_A0) {
        if (action == 3) {
            status = FeetechStsControl_StopMotion();
            if (status == HAL_OK) {
                FeetechStsControl_SetActionSource(owner);
                FeetechStsCommand_PrintCurrentTarget(source, "A0急停");
            } else {
                FeetechStsCommand_SendText(source, "A0急停失败：位置反馈无效");
            }
        } else if (action == 1) {
            if (FeetechStsControl_IsLimitSetting() != 0U) {
                status = FeetechStsControl_RecordCloseLimit();
                if (status == HAL_OK) {
                    FeetechStsCommand_PrintCurrentTarget(source, "闭合极限已记录");
                }
            } else if (FeetechStsControl_IsMotionState() != 0U) {
                status = FeetechStsControl_StopMotion();
                if (status == HAL_OK) {
                    FeetechStsControl_SetActionSource(owner);
                    FeetechStsCommand_PrintCurrentTarget(source, "A0急停");
                } else {
                    FeetechStsCommand_SendText(source, "A0急停失败：位置反馈无效");
                }
            } else if (FeetechStsControl_IsMotionRangeReady() != 0U) {
                FeetechStsCommand_PrintModeOnNextMotion(source);
                status = FeetechStsControl_StartGrip();
                if (status == HAL_OK) {
                    FeetechStsControl_SetActionSource(owner);
                    FeetechStsCommand_PrintCurrentTarget(source, "A0开始夹取");
                }
            } else {
                FeetechStsCommand_SendText(source, "未设置机械极限；发送 FF FF FE 03 F4 03 07 进入标定");
            }
        } else if (action == 2) {
            if (FeetechStsControl_IsLimitSetting() != 0U) {
                status = FeetechStsControl_RecordOpenLimit();
                if (status == HAL_OK) {
                    FeetechStsCommand_PrintCurrentTarget(source, "张开极限已记录");
                }
            } else if (FeetechStsControl_IsMotionRangeReady() != 0U) {
                FeetechStsCommand_PrintModeOnNextMotion(source);
                status = FeetechStsControl_ConfirmRelease();
                if (status == HAL_OK) {
                    FeetechStsControl_SetActionSource(owner);
                    FeetechStsCommand_PrintCurrentTarget(source, "A0开始张开");
                }
            } else {
                FeetechStsCommand_SendText(source, "未设置机械极限；发送 FF FF FE 03 F4 03 07 进入标定");
            }
        }
    } else if (command == CLAW_HOST_CMD_B0) {
        if (action == 0) {
            status = FeetechStsControl_StopSweep();
            if (status == HAL_OK) {
                FeetechStsControl_SetActionSource(owner);
                FeetechStsCommand_PrintCurrentTarget(source, "B0松手停止");
            } else {
                FeetechStsCommand_SendText(source, "B0停止失败：位置反馈无效");
            }
        } else if (action == 1) {
            FeetechStsCommand_PrintModeOnNextMotion(source);
            status = FeetechStsControl_StartSweep();
            if (status == HAL_OK) {
                FeetechStsControl_SetActionSource(owner);
            } else {
                FeetechStsCommand_SendText(source, "B0扫描失败：位置反馈无效");
            }
        } else if (action == 2) {
            status = FeetechStsControl_ReverseSweep();
            if (status == HAL_OK) {
                FeetechStsControl_SetActionSource(owner);

            }
        }
    } else if (command == CLAW_HOST_CMD_RANGE) {
        if (action == 0) {
            status = FeetechStsControl_SaveSweepRange(FEETECH_STS_SWEEP_RANGE_LIMITS);
        } else if (action == 1) {
            status = FeetechStsControl_SaveSweepRange(FEETECH_STS_SWEEP_RANGE_FULL_270);
        }
        if (status == HAL_OK) {
            s_mode_print_pending = 1U;
            FeetechStsCommand_SendText(source,
                (action == 0) ? "动作范围：机械极限" : "动作范围：0~270度");
        } else {
            FeetechStsCommand_SendText(source, "机械极限未设置；发送 FF FF FE 03 F4 03 07 进入标定");
        }
    }

    s_sts_command.last_status = status;
    return status;
}

// 解析 F5 上位机帧并进入统一动作入口。
static HAL_StatusTypeDef FeetechStsCommand_HandleHostCommand(const uint8_t *frame, uint8_t len, uint8_t source)
{
    ClawHostCommand_t command;
    HAL_StatusTypeDef status;
    int32_t action;

    if (ClawHostProtocol_Decode(frame, len, &command) == 0U) {
        return HAL_ERROR;
    }

    if (command.command == CLAW_HOST_CMD_ANGLE) {
        status = FeetechStsControl_CommandAngle(command.value, FeetechStsCommand_SourceToOwner(source));
        if (status != HAL_OK) {
            FeetechStsCommand_SendText(source, "角度控制失败：未设置机械极限；发送 FF FF FE 03 F4 03 07 进入标定");
        }
        s_sts_command.last_status = status;
        return status;
    }

    if (command.value < 0.0f) {
        return HAL_ERROR;
    }
    action = (int32_t)(command.value + 0.5f);
    if(Input_GetMode()==INPUT_PHYSICAL && action==1 &&
       (command.command==CLAW_HOST_CMD_A0||command.command==CLAW_HOST_CMD_B0||command.command==CLAW_HOST_CMD_PWM1_KEY||command.command==CLAW_HOST_CMD_PWM2_KEY))return HAL_ERROR;
    if ((command.command == CLAW_HOST_CMD_PWM1_KEY) ||
        (command.command == CLAW_HOST_CMD_PWM2_KEY)) {
        if (action > 1) {
            return HAL_ERROR;
        }
        status = PwmServoControl_SetVirtualKeyLevel(
            (command.command == CLAW_HOST_CMD_PWM1_KEY) ?
                PWM_SERVO_ID_1 : PWM_SERVO_ID_2,
            (action == 1) ? 1U : 0U);
        s_sts_command.last_status = status;
        return status;
    }
    if (((command.command == CLAW_HOST_CMD_A0) || (command.command == CLAW_HOST_CMD_B0)) &&
        (action <= 1)) {
        FeetechStsCommand_SetHostKeyLevel(command.command, (action == 1) ? 1U : 0U, source);
        s_sts_command.last_status = HAL_OK;
        return HAL_OK;
    }
    return FeetechStsCommand_ExecuteAction(command.command, action, source);
}

// 更新VOFA/蓝牙虚拟按键；同一按键只允许一个上位机来源保持。
static void FeetechStsCommand_SetHostKeyLevel(
    uint8_t command,
    uint8_t pressed,
    uint8_t source)
{
    pressed = (pressed != 0U) ? 1U : 0U;

    if ((source != FEETECH_STS_CMD_SOURCE_VOFA) &&
        (source != FEETECH_STS_CMD_SOURCE_BLUETOOTH)) {
        return;
    }

    if (command == CLAW_HOST_CMD_A0) {
        if(pressed)s_host_a_deadline=HAL_GetTick()+INPUT_MAX_HOLD_MS;
        if (pressed == 0U) {
            /*
             * 松开属于安全动作：
             * 任一上位机发送A0松开，都清除全部虚拟A0电平。
             */
            s_vofa_a0_pressed = 0U;
            s_bluetooth_a0_pressed = 0U;
        } else if (source == FEETECH_STS_CMD_SOURCE_VOFA) {
            /* VOFA接管A0，清除旧蓝牙A0电平。 */
            s_vofa_a0_pressed = 1U;
            s_bluetooth_a0_pressed = 0U;
        } else {
            /* 蓝牙接管A0，清除旧VOFA A0电平。 */
            s_bluetooth_a0_pressed = 1U;
            s_vofa_a0_pressed = 0U;
        }

        s_a0_event_source = source;
        Input_LegacyHeld(s_vofa_a0_pressed||s_vofa_b0_pressed||s_bluetooth_a0_pressed||s_bluetooth_b0_pressed);
        return;
    }

    if (command == CLAW_HOST_CMD_B0) {
        if(pressed)s_host_b_deadline=HAL_GetTick()+INPUT_MAX_HOLD_MS;
        if (pressed == 0U) {
            /* 任一上位机发送B0松开，都停止全部虚拟B0保持。 */
            s_vofa_b0_pressed = 0U;
            s_bluetooth_b0_pressed = 0U;
        } else if (source == FEETECH_STS_CMD_SOURCE_VOFA) {
            s_vofa_b0_pressed = 1U;
            s_bluetooth_b0_pressed = 0U;
        } else {
            s_bluetooth_b0_pressed = 1U;
            s_vofa_b0_pressed = 0U;
        }

        s_b0_event_source = source;
        Input_LegacyHeld(s_vofa_a0_pressed||s_vofa_b0_pressed||s_bluetooth_a0_pressed||s_bluetooth_b0_pressed);
    }
}

// 复位统一手势并同步当前有效电平。
static void FeetechStsCommand_ResetUnifiedGestures(uint32_t now)
{
    KeyGesture_Init(&s_a0_gesture);
    KeyGesture_Init(&s_b0_gesture);
    s_effective_a0_pressed = ((s_key_a0_pressed != 0U) || (s_vofa_a0_pressed != 0U) ||
                              (s_bluetooth_a0_pressed != 0U)) ? 1U : 0U;
    s_effective_b0_pressed = ((s_key_b0_pressed != 0U) || (s_vofa_b0_pressed != 0U) ||
                              (s_bluetooth_b0_pressed != 0U)) ? 1U : 0U;
    if (s_effective_a0_pressed != 0U) {
        KeyGesture_SetPressed(&s_a0_gesture, 1U, now);
    }
    if (s_effective_b0_pressed != 0U) {
        KeyGesture_SetPressed(&s_b0_gesture, 1U, now);
    }
    (void)KeyGesture_Task(&s_a0_gesture, now);
    (void)KeyGesture_Task(&s_b0_gesture, now);
}

// 组合键来源优先使用上位机最近输入，否则为实体按键。
static uint8_t FeetechStsCommand_CombinedSource(void)
{
    if ((s_a0_event_source == FEETECH_STS_CMD_SOURCE_VOFA) ||
        (s_b0_event_source == FEETECH_STS_CMD_SOURCE_VOFA)) {
        return FEETECH_STS_CMD_SOURCE_VOFA;
    }
    if ((s_a0_event_source == FEETECH_STS_CMD_SOURCE_BLUETOOTH) ||
        (s_b0_event_source == FEETECH_STS_CMD_SOURCE_BLUETOOTH)) {
        return FEETECH_STS_CMD_SOURCE_BLUETOOTH;
    }
    return FEETECH_STS_CMD_SOURCE_KEY;
}

// 动作来源：实体 A0/B0 分开记录，上位机按传输入口记录。
static FeetechStsControlOwner_t FeetechStsCommand_ActionOwner(uint8_t command, uint8_t source)
{
    if (source == FEETECH_STS_CMD_SOURCE_KEY) {
        return (command == CLAW_HOST_CMD_B0) ? FEETECH_STS_OWNER_KEY_B0 : FEETECH_STS_OWNER_KEY_A0;
    }
    return FeetechStsCommand_SourceToOwner(source);
}

// 下次运动打印一次当前扫描模式。
static void FeetechStsCommand_PrintModeOnNextMotion(uint8_t source)
{
    if (s_mode_print_pending == 0U) {
        return;
    }
    s_mode_print_pending = 0U;
    FeetechStsCommand_SendText(source,
        (FeetechStsControl_GetSweepRange() == FEETECH_STS_SWEEP_RANGE_LIMITS) ?
        "当前动作范围：机械极限" : "当前动作范围：0~270度");
}
// 命令来源：转换为控制权来源。
static FeetechStsControlOwner_t FeetechStsCommand_SourceToOwner(uint8_t source)
{
    if (source == FEETECH_STS_CMD_SOURCE_BLUETOOTH) {
        return FEETECH_STS_OWNER_BLUETOOTH;
    }

    if (source == FEETECH_STS_CMD_SOURCE_VOFA) {
        return FEETECH_STS_OWNER_VOFA;
    }

    if (source == FEETECH_STS_CMD_SOURCE_KEY) {
        return FEETECH_STS_OWNER_KEY_A0;
    }

    return FEETECH_STS_OWNER_USB_DIRECT;
}

// 异步动作信息返回当前动作来源；实体按键仍输出到 VOFA。
static uint8_t FeetechStsCommand_ActionOutputSource(void)
{
    if (FeetechStsControl_GetOwner() == FEETECH_STS_OWNER_BLUETOOTH) {
        return FEETECH_STS_CMD_SOURCE_BLUETOOTH;
    }
    return FEETECH_STS_CMD_SOURCE_VOFA;
}

#if (FEETECH_STS_COMMAND_ECHO_RX_FRAME != 0U)
// 回显官方帧。
static void FeetechStsCommand_SendResponse(const uint8_t *frame, uint8_t len, uint8_t source)
{
    if ((frame == 0) || (len == 0U)) {
        return;
    }

    if (source == FEETECH_STS_CMD_SOURCE_BLUETOOTH) {
        (void)Bluetooth_SendFeetechFrame(frame, len);
        s_sts_command.response_count++;
    } else if (source == FEETECH_STS_CMD_SOURCE_VOFA) {
        (void)VOFA_Protocol_SendFeetechFrame(frame, len);
        s_sts_command.response_count++;
    }
}
#endif

// 发送文本。
static void FeetechStsCommand_SendText(uint8_t source, const char *text)
{
    uint16_t len;
    static const char crlf[] = "\r\n";

    if (text == 0) {
        return;
    }

    len = (uint16_t)strlen(text);

    if (source == FEETECH_STS_CMD_SOURCE_BLUETOOTH) {
        if (Bluetooth_SendTextLine(text) == BSP_BT_OK) {
            s_sts_command.response_count++;
        }
    } else if ((source == FEETECH_STS_CMD_SOURCE_VOFA) ||
               (source == FEETECH_STS_CMD_SOURCE_KEY) ||
               (source == FEETECH_STS_CMD_SOURCE_NONE)) {
        (void)VOFA_Protocol_SendRaw((const uint8_t *)text, len);
        (void)VOFA_Protocol_SendRaw((const uint8_t *)crlf, 2U);
        s_sts_command.response_count++;
    }
}

// 格式化发送文本。
static void FeetechStsCommand_Printf(uint8_t source, const char *fmt, ...)
{
    char text[FEETECH_STS_COMMAND_STATUS_TEXT_MAX];
    va_list args;

    if (fmt == 0) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    text[sizeof(text) - 1U] = '\0';
    FeetechStsCommand_SendText(source, text);
}

// A0 夹取确认调试信息打印。
static void FeetechStsCommand_TaskGripDetectPrint(void)
{
    FeetechStsGripDetectEvent_t event;
#if (FEETECH_STS_CONTROL_ADAPTIVE_GRIP_ENABLE != 0U)
    const FeetechStsControlStatus_t *control;
    const FeetechStsGripConfig_t *config;
    uint16_t angle;
    uint16_t target;

    if (FeetechStsControl_TakeGripDetectEvent(&event) == 0U) {
        return;
    }
    control = FeetechStsControl_GetStatus();
    config = (control != 0) ? FeetechStsControl_GetProfileConfig(control->profile) : 0;
    if ((config == 0) || (event.confirm_count < config->trigger_count)) {
        return;
    }

    angle = (event.angle_deg > 0.0f) ? (uint16_t)(event.angle_deg + 0.5f) : 0U;
    target = (event.target_angle_deg > 0.0f) ? (uint16_t)(event.target_angle_deg + 0.5f) : 0U;
    FeetechStsCommand_Printf(FeetechStsCommand_ActionOutputSource(),
                             "夹取到 %u° --> %u° 当前=%u°",
                             angle,
                             target,
                             angle);
#else
    (void)FeetechStsControl_TakeGripDetectEvent(&event);
#endif
}

// B0 扫描打印事件。
static void FeetechStsCommand_TaskSweepPrint(void)
{
    FeetechStsSweepPrintEvent_t event;
    uint16_t angle;
    uint16_t target;

    if (FeetechStsControl_TakeSweepPrintEvent(&event) == 0U) {
        return;
    }

    angle = (event.angle_deg > 0.0f) ? (uint16_t)(event.angle_deg + 0.5f) : 0U;
    target = (event.target_angle_deg > 0.0f) ? (uint16_t)(event.target_angle_deg + 0.5f) : 0U;
    FeetechStsCommand_Printf(FeetechStsCommand_ActionOutputSource(),
                             "%s 当前=%u° 目标=%u°",
                             event.text,
                             angle,
                             target);
    
    s_sweep_progress_print_tick_ms = HAL_GetTick();
}

// B0扫描过程定时打印真实角度和编码。
static void FeetechStsCommand_TaskSweepProgressPrint(
    uint32_t now)
{
    const FeetechStsControlStatus_t *control;
    const FeetechStsFeedback_t *feedback;
    uint16_t angle_tenths;

    control = FeetechStsControl_GetStatus();

    /*
     * 只在真实B0扫描状态打印。
     * B0松开后立即停止，不允许残留打印。
     */
    if ((control == 0) ||
        (control->sweep_active == 0U) ||
        (control->state != FEETECH_STS_CONTROL_SWEEP)) {
        s_sweep_progress_print_tick_ms = now;
        s_sweep_progress_feedback_tick_ms = 0UL;
        return;
    }

    if ((uint32_t)(now -
        s_sweep_progress_print_tick_ms) <
        FEETECH_STS_COMMAND_SWEEP_PROGRESS_MS) {
        return;
    }

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback == 0) ||
        (feedback->position_valid == 0U)) {
        return;
    }

    /* 不重复打印同一包反馈。 */
    if (feedback->last_update_tick ==
        s_sweep_progress_feedback_tick_ms) {
        return;
    }

    /* 只打印较新的反馈。 */
    if ((uint32_t)(now - feedback->last_update_tick) >
        FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS) {
        return;
    }

    s_sweep_progress_print_tick_ms = now;
    s_sweep_progress_feedback_tick_ms =
        feedback->last_update_tick;

    angle_tenths =
        (feedback->angle_deg > 0.0f) ?
        (uint16_t)(feedback->angle_deg * 10.0f + 0.5f) :
        0U;

    FeetechStsCommand_Printf(
        FeetechStsCommand_ActionOutputSource(),
        "B0扫描 当前=%u.%u° 编码=%u",
        angle_tenths / 10U,
        angle_tenths % 10U,
        feedback->position);
}

// 打印正常到位或卡死停止事件。
static void FeetechStsCommand_TaskMotionPrint(void)
{
    FeetechStsMotionPrintEvent_t event;
    uint16_t angle_tenths;

    if (FeetechStsControl_TakeMotionPrintEvent(&event) == 0U) {
        return;
    }
    angle_tenths = (event.angle_deg > 0.0f) ? (uint16_t)(event.angle_deg * 10.0f + 0.5f) : 0U;
    if (event.stalled != 0U) {
        FeetechStsCommand_Printf(FeetechStsCommand_ActionOutputSource(),
            "卡死：当前%u.%u°，编码%u，目标编码%u",
            angle_tenths / 10U,
            angle_tenths % 10U,
            event.position,
            event.target_position);
    } else {
        FeetechStsCommand_Printf(FeetechStsCommand_ActionOutputSource(),
            "到位 当前=%u° 编码=%u",
            (uint16_t)(event.angle_deg + 0.5f),
            event.position);
    }
}
// VOFA 四项反馈：当前位置、目标、电压、电流。
static void FeetechStsCommand_TaskManagerPrint(uint32_t now)
{
#if (FEETECH_STS_COMMAND_VOFA_TELEMETRY_ENABLE != 0U)

    const FeetechStsManager_t *manager;
    uint16_t period_ms;

    manager = FeetechStsControl_GetManager();
    if ((manager == 0) || (manager->option.print_enable == 0U)) {
        return;
    }
    period_ms = manager->option.print_period_ms;
    if (period_ms == 0U) {
        period_ms = 1U;
    }
    if ((uint32_t)(now - s_vofa_telemetry_tick_ms) < period_ms) {
        return;
    }
    s_vofa_telemetry_tick_ms = now;
    FeetechStsCommand_SendTelemetryOnce(s_telemetry_source);
#else
    (void)now;
#endif
}

// 单次打印位置、目标、电压和电流。
static void FeetechStsCommand_SendTelemetryOnce(uint8_t source)
{
    const FeetechStsManager_t *manager;

    manager = FeetechStsControl_GetManager();
    if (manager == 0) {
        return;
    }
    FeetechStsCommand_Printf(source,
                             "claw:%u,%u,%u,%u",
                             (uint16_t)(manager->feedback.angle_deg + 0.5f),
                             (uint16_t)(manager->target.target_angle_deg + 0.5f),
                             manager->feedback.voltage_mv,
                             manager->feedback.current_ma);
}

//输出最近发送帧信息。
static void FeetechStsCommand_SendCommandSentText(const uint8_t *frame, uint8_t len, uint8_t source, HAL_StatusTypeDef status)
{
    uint8_t instruction;
    uint8_t address;
    uint8_t data_len;
    uint16_t pos;
    uint16_t time_ms;
    uint16_t speed;

    if ((frame == 0) || (len < 6U)) {
        return;
    }

    instruction = frame[4];
    address = (len >= 7U) ? frame[5] : 0U;
    data_len = (frame[3] >= 3U) ? (uint8_t)(frame[3] - 3U) : 0U;

    if (((instruction == FEETECH_STS_INST_WRITE) || (instruction == FEETECH_STS_INST_REG_WRITE)) &&
        (address == FEETECH_STS_ADDR_GOAL_POSITION_L) && (data_len >= 6U) && (len >= 12U)) {
        pos = FeetechSts_MakeU16(frame[6], frame[7]);
        time_ms = FeetechSts_MakeU16(frame[8], frame[9]);
        speed = FeetechSts_MakeU16(frame[10], frame[11]);
        if (status == HAL_OK) {
            FeetechStsCommand_Printf(source,
                                     "发送 %s 角度=%u° 时间=%ums 速度=%u",
                                     FeetechStsCommand_InstructionNameCn(instruction),
                                     (uint16_t)(FeetechSts_PositionToAngle(pos) + 0.5f),
                                     time_ms,
                                     speed);
        } else {
            FeetechStsCommand_Printf(source,
                                     "错误 发送%s 角度=%u° 时间=%ums 速度=%u %s",
                                     FeetechStsCommand_InstructionNameCn(instruction),
                                     (uint16_t)(FeetechSts_PositionToAngle(pos) + 0.5f),
                                     time_ms,
                                     speed,
                                     FeetechStsCommand_HalNameCn(status));
        }
        return;
    }

    if (status != HAL_OK) {
        FeetechStsCommand_Printf(source,
                                 "错误 发送%s 地址=%u 长度=%u %s",
                                 FeetechStsCommand_InstructionNameCn(instruction),
                                 address,
                                 data_len,
                                 FeetechStsCommand_HalNameCn(status));
    }
    
    if (((instruction == FEETECH_STS_INST_WRITE) ||
         (instruction == FEETECH_STS_INST_REG_WRITE)) &&
        (status == HAL_OK)) {

        FeetechStsCommand_PrintWriteSummary(
            frame,
            len,
            source,
            (instruction == FEETECH_STS_INST_REG_WRITE) ?
            "异步写入已发送" :
            "写入已发送");

        return;
    }
}

//输出最近回包信息。
static void FeetechStsCommand_SendCommandResponseText(const uint8_t *frame, uint8_t frame_len, const uint8_t *response, uint8_t response_len, uint8_t source, HAL_StatusTypeDef status)
{
    uint8_t instruction;
    uint8_t address;
    uint8_t read_len;
    char prefix[48];

    if ((frame == 0) || (frame_len < 6U)) {
        return;
    }

    instruction = frame[4];
    address = (frame_len >= 7U) ? frame[5] : 0U;
    read_len = (frame_len >= 8U) ? frame[6] : 0U;

    if ((status != HAL_OK) || (response == 0) || (response_len == 0U)) {
        if ((instruction == FEETECH_STS_INST_WRITE) ||
            (instruction == FEETECH_STS_INST_REG_WRITE)) {
            FeetechStsCommand_SendText(source, "写入帧已发送，但未收到ACK");
        } else {
            FeetechStsCommand_Printf(source,
                                     "超时 %s 地址=0x%02X 长度=%u %s",
                                     FeetechStsCommand_InstructionNameCn(instruction),
                                     address,
                                     read_len,
                                     FeetechStsCommand_HalNameCn(status));
        }
        return;
    }


    if ((response_len < 6U) || (FeetechSts_IsValidFrame(response, response_len) == 0U)) {
        FeetechStsCommand_Printf(source, "回包格式错误 长度=%u", response_len);
        return;
    }

    if (instruction == FEETECH_STS_INST_PING) {
        (void)snprintf(prefix, sizeof(prefix), "PING成功 ID=%u", response[2]);
        FeetechStsCommand_PrintServoStatus(source, prefix, response[4]);
        return;
    }

    if (instruction == FEETECH_STS_INST_READ) {
        if (response[4] != 0U) {
            FeetechStsCommand_PrintServoStatus(source, "READ", response[4]);
        }
        (void)FeetechStsCommand_PrintReadResponse(address,
                                                   read_len,
                                                   response,
                                                   response_len,
                                                   source);
        return;
    }

    if ((instruction == FEETECH_STS_INST_WRITE) ||
        (instruction == FEETECH_STS_INST_REG_WRITE)) {
        if (response[4] == 0U) {
            FeetechStsCommand_PrintWriteSummary(frame, frame_len, source,(instruction == FEETECH_STS_INST_REG_WRITE) ?"异步写入已暂存" :"写入成功");
        } else {
            FeetechStsCommand_PrintServoStatus(source, "写入已收到", response[4]);
        }
        return;
    }

    FeetechStsCommand_PrintServoStatus(source,
                                       FeetechStsCommand_InstructionNameCn(instruction),
                                       response[4]);
}
//处理本地扩展命令。
static uint8_t FeetechStsCommand_HandleLocalFrame(const uint8_t *frame, uint8_t len, uint8_t source)
{
    /* 处理本地扩展指令，不转发到舵机总线。 */
    uint8_t instruction;
    uint8_t param_len;
    const uint8_t *params;
    uint8_t param;

    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }

    instruction = frame[4];
    if ((instruction != FEETECH_STS_INST_LOCAL_STATUS_PRINT) &&
        (instruction != FEETECH_STS_INST_LOCAL_EXAMPLE_PRINT) &&
        (instruction != FEETECH_STS_INST_LOCAL_PARAM) &&
        (instruction != FEETECH_STS_INST_LOCAL_AUTHORITY) &&
        (instruction != FEETECH_STS_INST_LOCAL_SAFETY) &&
        (instruction != CLAW_HOST_INST_ACTION)) {
        return 0U;
    }

    param_len = (frame[3] >= 2U) ? (uint8_t)(frame[3] - 2U) : 0U;
    params = (param_len == 0U) ? 0 : &frame[5];

    if (instruction == CLAW_HOST_INST_ACTION) {
        s_sts_command.last_status = FeetechStsCommand_HandleHostCommand(frame, len, source);
        return 1U;
    }

    if (instruction == FEETECH_STS_INST_LOCAL_STATUS_PRINT) {
        param = ((params != 0) && (param_len >= 1U)) ? params[0] : 2U;
        if (param == 0U) {
            FeetechStsControl_SetPrintEnable(0U);
            FeetechStsCommand_Printf(source, "claw周期打印已关闭");
        } else if (param == 1U) {
            FeetechStsControl_SetPrintEnable(1U);
            s_telemetry_source = source;
            s_vofa_telemetry_tick_ms = 0UL;
            FeetechStsCommand_Printf(source, "claw周期打印已开启");
        } else if (param == 2U) {
            FeetechStsCommand_SendTelemetryOnce(source);
        } else {
            FeetechStsCommand_Printf(source, "错误 未知打印动作 %u", param);
            s_sts_command.last_status = HAL_ERROR;
        }
        return 1U;
    }

    if (instruction == FEETECH_STS_INST_LOCAL_EXAMPLE_PRINT) {
        FeetechStsCommand_SendExampleText(source);
        return 1U;
    }

    if (instruction == FEETECH_STS_INST_LOCAL_PARAM) {
        FeetechStsCommand_HandleParamCommand(params, param_len, source);
        return 1U;
    }

    if (instruction == FEETECH_STS_INST_LOCAL_AUTHORITY) {
        FeetechStsCommand_HandleAuthorityCommand(params, param_len, source);
        return 1U;
    }

    if (instruction == FEETECH_STS_INST_LOCAL_SAFETY) {
        FeetechStsCommand_HandleSafetyCommand(params, param_len, source);
        return 1U;
    }

    return 0U;
}

// 机械极限命令：查询、标定、取消和清除。
static void FeetechStsCommand_HandleSafetyCommand(const uint8_t *params, uint8_t param_len, uint8_t source)
{
    uint8_t action;
    HAL_StatusTypeDef status;

    action = ((params != 0) && (param_len >= 1U)) ? params[0] : 0U;
    status = HAL_OK;

    switch (action) {
        case 0U:
            FeetechStsCommand_Printf(source,
                                     "标定%s 极限%s B0=%s",
                                     (FeetechStsControl_IsLimitSetting() != 0U) ? "中" : "关",
                                     (FeetechStsControl_AreLimitsValid() != 0U) ? "有效" : "未设置",
                                     (FeetechStsControl_GetSweepRange() == FEETECH_STS_SWEEP_RANGE_LIMITS) ? "机械极限" : "0~270度");
            break;
        case 3U:
            status = FeetechStsControl_BeginLimitSetting();
            FeetechStsCommand_SendText(source, (status == HAL_OK) ? "进入极限标定：B0长按扫描，A0单击闭合端，双击张开端" : "进入极限标定失败");
            break;
        case 4U:
            status = FeetechStsControl_CancelLimitSetting();
            FeetechStsCommand_SendText(source, (status == HAL_OK) ? "已取消极限标定" : "取消极限标定失败");
            break;
        case 5U:
            status = FeetechStsControl_ClearStoredLimits();
            FeetechStsCommand_SendText(source, (status == HAL_OK) ? "机械极限已清除" : "机械极限清除失败");
            break;
        case 6U:
            status = FeetechStsControl_FinishLimitSetting();
            FeetechStsCommand_SendText(source, (status == HAL_OK) ? "机械极限已校验并保存" : "机械极限保存失败");
            break;
        default:
            status = HAL_ERROR;
            FeetechStsCommand_SendText(source, "未知机械极限指令");
            break;
    }

    s_sts_command.last_status = status;
    FeetechStsCommand_SendSafetyAck(source, action, status);
}

// 返回 F4 二进制应答。
static void FeetechStsCommand_SendSafetyAck(uint8_t source, uint8_t action, HAL_StatusTypeDef status)
{
    uint8_t frame[11];

    frame[0] = FEETECH_STS_FRAME_HEADER;
    frame[1] = FEETECH_STS_FRAME_HEADER;
    frame[2] = FEETECH_STS_BROADCAST_ID;
    frame[3] = 7U;
    frame[4] = FEETECH_STS_INST_LOCAL_SAFETY;
    frame[5] = action;
    frame[6] = (uint8_t)status;
    frame[7] = FeetechStsControl_IsLimitSetting();
    frame[8] = FeetechStsControl_AreLimitsValid();
    frame[9] = (uint8_t)FeetechStsControl_GetSweepRange();
    frame[10] = FeetechSts_CalcChecksum(frame, sizeof(frame));

    if (source == FEETECH_STS_CMD_SOURCE_BLUETOOTH) {
        (void)Bluetooth_SendFeetechFrame(frame, sizeof(frame));
        s_sts_command.response_count++;
    } else if (source == FEETECH_STS_CMD_SOURCE_VOFA) {
        (void)VOFA_Protocol_SendFeetechFrame(frame, sizeof(frame));
        s_sts_command.response_count++;
    }
}

// 发送示例文本。
static void FeetechStsCommand_SendExampleText(uint8_t source)
{
    FeetechStsCommand_Printf(source, "[示例] 查询ID1在线 = FF FF 01 02 01 FB");
    FeetechStsCommand_Printf(source, "[示例] 读当前位置 = FF FF 01 04 02 38 02 BE");
    FeetechStsCommand_Printf(source, "[示例] 读综合反馈 = FF FF 01 04 02 38 0F B1");
    FeetechStsCommand_Printf(source, "[示例] 读最高/最低输入电压 = FF FF 01 04 02 0E 02 E8");
    FeetechStsCommand_Printf(source, "[示例] 解锁EEPROM = FF FF 01 04 03 37 00 C0");
    FeetechStsCommand_Printf(source, "[示例] 最高输入电压设为11.7V = FF FF 01 04 03 0E 75 74");
    FeetechStsCommand_Printf(source, "[示例] 锁定EEPROM = FF FF 01 04 03 37 01 BF");
    FeetechStsCommand_Printf(source, "[示例] 写目标位置2048、时间0、速度1000 = FF FF 01 09 03 2A 00 08 00 00 E8 03 D5");
    FeetechStsCommand_Printf(source, "[示例] 舵机上力 = FF FF 01 04 03 28 01 CE");
    FeetechStsCommand_Printf(source, "[示例] 关闭claw周期打印 = FF FF FE 03 F0 00 0E");
    FeetechStsCommand_Printf(source, "[示例] 开启claw周期打印 = FF FF FE 03 F0 01 0D");
    FeetechStsCommand_Printf(source, "[示例] 单次打印claw = FF FF FE 03 F0 02 0C");
    FeetechStsCommand_Printf(source, "[示例] 列出全部参数 = FF FF FE 03 F2 00 0C");
    FeetechStsCommand_Printf(source, "[示例] 设置张开速度900 = FF FF FE 06 F2 02 04 84 03 7C");
    FeetechStsCommand_Printf(source, "[示例] 设置扫描步进周期40ms = FF FF FE 06 F2 02 08 28 00 D7");
    FeetechStsCommand_Printf(source, "[示例] 查询机械极限状态 = FF FF FE 03 F4 00 0A");
    FeetechStsCommand_Printf(source, "[示例] 进入极限标定 = FF FF FE 03 F4 03 07");
    FeetechStsCommand_Printf(source, "[示例] 保存极限标定 = FF FF FE 03 F4 06 04");
    FeetechStsCommand_Printf(source, "[示例] 角度滑块 = FF FF FE 07 F5 00 %% 00");
    FeetechStsCommand_Printf(source, "[示例] A0按钮 = FF FF FE 07 F5 01 %% 00");
    FeetechStsCommand_Printf(source, "[示例] B0按钮 = FF FF FE 07 F5 02 %% 00");
}

// 发送参数列表。
static void FeetechStsCommand_SendParamList(uint8_t source)
{
    uint8_t i;

    for (i = 0U; i < FeetechStsControl_GetParamCount(); i++) {
        FeetechStsCommand_SendParamInfo(source, (FeetechStsParamId_t)i);
    }
}

// 发送单个参数信息。
static void FeetechStsCommand_SendParamInfo(uint8_t source, FeetechStsParamId_t id)
{
    FeetechStsParamInfo_t info;

    if (FeetechStsControl_GetParamInfo(id, &info) == 0U) {
        FeetechStsCommand_Printf(source, "错误 参数不存在 %u", (uint8_t)id);
        return;
    }

    FeetechStsCommand_Printf(source,
                             "参数 %s：%ld",
                             FeetechStsCommand_ParamNameCn(info.id),
                             (long)info.value);
}

// 参数命令：查询、设置或恢复参数。
static void FeetechStsCommand_HandleParamCommand(const uint8_t *params, uint8_t param_len, uint8_t source)
{
    uint8_t action;
    FeetechStsParamId_t id;
    int32_t value;

    action = ((params != 0) && (param_len >= 1U)) ? params[0] : 0U;

    if (action == 0U) {
        FeetechStsCommand_SendParamList(source);
        return;
    }

    if (action == 4U) {
        FeetechStsControl_ResetAllParams();
        FeetechStsCommand_Printf(source, "参数 已恢复全部默认值");
        return;
    }

    if ((params == 0) || (param_len < 2U)) {
        FeetechStsCommand_Printf(source, "错误 缺少参数");
        return;
    }

    id = (FeetechStsParamId_t)params[1];

    if (action == 1U) {
        FeetechStsCommand_SendParamInfo(source, id);
        return;
    }

    if (action == 3U) {
        if (FeetechStsControl_ResetParamValue(id) != 0U) {
            FeetechStsCommand_SendParamInfo(source, id);
        } else {
            FeetechStsCommand_Printf(source, "错误 参数恢复失败 %u", (uint8_t)id);
        }
        return;
    }

    if (action == 2U) {
        if (param_len >= 6U) {
            value = (int32_t)((uint32_t)params[2] |
                              ((uint32_t)params[3] << 8) |
                              ((uint32_t)params[4] << 16) |
                              ((uint32_t)params[5] << 24));
        } else if (param_len >= 4U) {
            value = (int32_t)(int16_t)FeetechSts_MakeU16(params[2], params[3]);
        } else {
            FeetechStsCommand_Printf(source, "错误 缺少参数值");
            return;
        }

        if (FeetechStsControl_SetParamValue(id, value) != 0U) {
            FeetechStsCommand_SendParamInfo(source, id);
        } else {
            FeetechStsCommand_Printf(source, "错误 参数设置失败 %u", (uint8_t)id);
        }
        return;
    }

    FeetechStsCommand_Printf(source, "错误 未知参数动作 %u", action);
}

// 兼容旧控制权命令；控制来源由最新运动命令自动更新。
static void FeetechStsCommand_HandleAuthorityCommand(const uint8_t *params, uint8_t param_len, uint8_t source)
{
    (void)params;
    (void)param_len;
    FeetechStsCommand_Printf(source, "无需切换控制权，最新运动命令自动生效");
}

// 指令码：转换为中文名称。
static const char *FeetechStsCommand_InstructionNameCn(uint8_t instruction)
{
    switch (instruction) {
        case FEETECH_STS_INST_PING:                 return "查询状态";
        case FEETECH_STS_INST_READ:                 return "读取数据";
        case FEETECH_STS_INST_WRITE:                return "写入数据";
        case FEETECH_STS_INST_REG_WRITE:            return "异步写入";
        case FEETECH_STS_INST_ACTION:               return "执行异步写";
        case FEETECH_STS_INST_RECOVERY:             return "恢复参数";
        case FEETECH_STS_INST_RESET:                return "重置状态";
        case FEETECH_STS_INST_SYNC_READ:            return "同步读取";
        case FEETECH_STS_INST_SYNC_WRITE:           return "同步写入";
        case FEETECH_STS_INST_LOCAL_STATUS_PRINT:   return "本地状态打印";
        case FEETECH_STS_INST_LOCAL_EXAMPLE_PRINT:  return "打印示例指令";
        case FEETECH_STS_INST_LOCAL_PARAM:          return "本地参数设置";
        case FEETECH_STS_INST_LOCAL_AUTHORITY:      return "控制权兼容";
        case FEETECH_STS_INST_LOCAL_SAFETY:         return "机械极限管理";
        case CLAW_HOST_INST_ACTION:                   return "夹爪动作";
        default:                                    return "未知指令";
    }
}

// 参数 ID：转换为中文名称。
static const char *FeetechStsCommand_ParamNameCn(FeetechStsParamId_t id)
{
    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:           return "张开角度";
        case FEETECH_STS_PARAM_CLOSE_ANGLE:          return "闭合角度";
        case FEETECH_STS_PARAM_PROFILE:              return "夹取档位";
        case FEETECH_STS_PARAM_DEFAULT_SPEED:        return "默认速度";
        case FEETECH_STS_PARAM_OPEN_SPEED:           return "张开速度";
        case FEETECH_STS_PARAM_CLOSE_SPEED:          return "夹取速度";
        case FEETECH_STS_PARAM_SWEEP_SPEED:          return "扫描速度";
        case FEETECH_STS_PARAM_SWEEP_STEP:           return "扫描步进角度";
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:        return "扫描步进周期";
        case FEETECH_STS_PARAM_FULL_TORQUE:          return "运动转矩";
        case FEETECH_STS_PARAM_HOLD_TORQUE:          return "保持转矩";
        case FEETECH_STS_PARAM_FEEDBACK_MS:          return "反馈周期";
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS:     return "空闲反馈周期";
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:       return "旧有效位移";
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:       return "稳定确认时间";
        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:       return "低电压阈值";
        case FEETECH_STS_PARAM_SLIP_ANGLE:           return "防滑角度";
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:      return "补夹角度";
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:   return "释放允许";
        case FEETECH_STS_PARAM_PRINT_ENABLE:         return "打印开关";
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:      return "打印周期";
        default:                                     return "未知参数";
    }
}

// HAL 状态：转换为中文名称。
static const char *FeetechStsCommand_HalNameCn(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:      return "成功";
        case HAL_ERROR:   return "错误";
        case HAL_BUSY:    return "忙";
        case HAL_TIMEOUT: return "超时";
        default:          return "未知";
    }
}

// 原因字符串：转换为中文说明。
// 打印当前角和目标角。
static void FeetechStsCommand_PrintCurrentTarget(uint8_t source, const char *text)
{
    const FeetechStsFeedback_t *feedback;

    float target;
    uint32_t now;

    now = HAL_GetTick();
    feedback = FeetechStsBus_GetFeedback();

    target = FeetechStsControl_GetTargetAngle();

    if ((feedback != 0) && (feedback->position_valid != 0U) &&
        ((uint32_t)(now - feedback->last_update_tick) <= FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS)) {
        FeetechStsCommand_Printf(source,
                                 "%s 当前=%u° 编码=%u 目标=%u°",
                                 (text != 0) ? text : "动作",
                                 (uint16_t)(feedback->angle_deg + 0.5f),
                                 feedback->position,
                                 (uint16_t)(target + 0.5f));
    } else {
        FeetechStsCommand_Printf(source,
                                 "%s 当前=无反馈 目标=%u°",
                                 (text != 0) ? text : "动作",
                                 (uint16_t)(target + 0.5f));
    }
}

/*============================================================
 * 6. 内部辅助判断
 *============================================================*/

/*============================================================
 * 6. 内部辅助判断
 *============================================================*/

// 判断是否为写入类帧。
static uint8_t FeetechStsCommand_IsWriteLikeFrame(const uint8_t *frame, uint8_t len)
{
    uint8_t instruction;

    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }

    instruction = frame[4];

    switch (instruction) {
        case FEETECH_STS_INST_WRITE:
        case FEETECH_STS_INST_REG_WRITE:
        case FEETECH_STS_INST_ACTION:
        case FEETECH_STS_INST_SYNC_WRITE:
        case FEETECH_STS_INST_RECOVERY:
        case FEETECH_STS_INST_RESET:
            return 1U;

        default:
            return 0U;
    }
}

// 判断外部帧是否会改变物理运动目标。
static uint8_t FeetechStsCommand_IsControlFrame(const uint8_t *frame, uint8_t len)
{
    uint8_t instruction;
    uint8_t address;
    uint8_t data_len;
    uint8_t item_len;

    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }

    instruction = frame[4];
    if (((instruction == FEETECH_STS_INST_WRITE) ||
         (instruction == FEETECH_STS_INST_REG_WRITE)) &&
        (len >= 7U) &&
        (frame[3] >= 3U)) {
        address = frame[5];
        data_len = (uint8_t)(frame[3] - 3U);
        return FeetechStsCommand_WriteTouchesTarget(address, data_len);
    }

    if ((instruction == FEETECH_STS_INST_SYNC_WRITE) &&
        (len >= 8U) &&
        (frame[3] >= 4U)) {
        address = frame[5];
        item_len = frame[6];
        return FeetechStsCommand_WriteTouchesTarget(address, item_len);
    }

    if ((instruction == FEETECH_STS_INST_ACTION) &&
        (s_pending_reg_target.valid != 0U)) {
        return 1U;
    }

    if ((instruction == FEETECH_STS_INST_RECOVERY) ||
        (instruction == FEETECH_STS_INST_RESET)) {
        return 1U;
    }

    return 0U;
}


// 同步外部目标到控制层。
static void FeetechStsCommand_SyncExternalTarget(const uint8_t *frame, uint8_t len, uint8_t source)
{
    FeetechStsCommandTarget_t target;
    uint8_t instruction;

    if ((frame == 0) || (len < 6U)) {
        return;
    }

    instruction = frame[4];

    if ((instruction == FEETECH_STS_INST_RECOVERY) ||
        (instruction == FEETECH_STS_INST_RESET)) {
        s_pending_reg_target.valid = 0U;
        return;
    }

    if (instruction == FEETECH_STS_INST_ACTION) {
        if (s_pending_reg_target.valid != 0U) {
            target = s_pending_reg_target;
            s_pending_reg_target.valid = 0U;
            (void)FeetechStsControl_ApplyExternalTarget(target.angle_deg,
                                                        target.time_ms,
                                                        target.speed,
                                                        target.acc,
                                                        FeetechStsCommand_SourceToOwner(source));
        }
        return;
    }

    if (FeetechStsCommand_ParseTargetFrame(frame, len, &target) == 0U) {
        return;
    }

    if (instruction == FEETECH_STS_INST_REG_WRITE) {
        s_pending_reg_target = target;
        s_pending_reg_target.valid = 1U;
        return;
    }

    s_pending_reg_target.valid = 0U;
    (void)FeetechStsControl_ApplyExternalTarget(target.angle_deg,
                                                target.time_ms,
                                                target.speed,
                                                target.acc,
                                                FeetechStsCommand_SourceToOwner(source));
}

// 解析官方目标帧。
static uint8_t FeetechStsCommand_ParseTargetFrame(const uint8_t *frame, uint8_t len, FeetechStsCommandTarget_t *out_target)
{
    uint8_t instruction;
    uint8_t address;
    uint8_t data_len;
    uint8_t item_len;
    uint8_t param_len;

    if ((frame == 0) || (out_target == 0) || (len < 6U)) {
        return 0U;
    }

    memset(out_target, 0, sizeof(*out_target));
    instruction = frame[4];

    if (((instruction == FEETECH_STS_INST_WRITE) ||
         (instruction == FEETECH_STS_INST_REG_WRITE)) &&
        (len >= 7U) &&
        (frame[3] >= 3U)) {
        address = frame[5];
        data_len = (uint8_t)(frame[3] - 3U);
        if (len < (uint8_t)(6U + data_len)) {
            return 0U;
        }
        return FeetechStsCommand_ParseWriteTarget(address, &frame[6], data_len, out_target);
    }

    if ((instruction == FEETECH_STS_INST_SYNC_WRITE) &&
        (len >= 9U) &&
        (frame[3] >= 5U)) {
        address = frame[5];
        item_len = frame[6];
        param_len = (uint8_t)(frame[3] - 4U);
        if ((item_len == 0U) || (param_len < (uint8_t)(item_len + 1U))) {
            return 0U;
        }
        return FeetechStsCommand_ParseWriteTarget(address, &frame[8], item_len, out_target);
    }

    return 0U;
}

// 解析目标位置字段。
static uint8_t FeetechStsCommand_ParseWriteTarget(uint8_t address, const uint8_t *data, uint8_t data_len, FeetechStsCommandTarget_t *out_target)
{
    uint16_t start;
    uint16_t end;
    uint16_t offset;
    uint16_t pos;

    if ((data == 0) || (out_target == 0) || (data_len == 0U)) {
        return 0U;
    }

    start = address;
    end = (uint16_t)address + (uint16_t)data_len - 1U;
    if ((start > (uint16_t)FEETECH_STS_ADDR_GOAL_POSITION_L) ||
        (end < (uint16_t)(FEETECH_STS_ADDR_GOAL_POSITION_L + 1U))) {
        return 0U;
    }

    offset = (uint16_t)FEETECH_STS_ADDR_GOAL_POSITION_L - start;
    if ((offset + 1U) >= data_len) {
        return 0U;
    }

    pos = FeetechSts_MakeU16(data[offset], data[(uint8_t)(offset + 1U)]);
    out_target->angle_deg = FeetechSts_PositionToAngle(pos);
    out_target->time_ms = 0U;
    out_target->speed = 0U;
    out_target->acc = FEETECH_STS_DEFAULT_ACC;

    if ((start <= FEETECH_STS_ADDR_ACC) &&
        (end >= FEETECH_STS_ADDR_ACC)) {
        out_target->acc = data[(uint8_t)((uint16_t)FEETECH_STS_ADDR_ACC - start)];
    }

    if ((start <= FEETECH_STS_ADDR_GOAL_TIME_L) &&
        (end >= (uint16_t)(FEETECH_STS_ADDR_GOAL_TIME_L + 1U))) {
        offset = (uint16_t)FEETECH_STS_ADDR_GOAL_TIME_L - start;
        out_target->time_ms = FeetechSts_MakeU16(data[offset], data[(uint8_t)(offset + 1U)]);
    }

    if ((start <= FEETECH_STS_ADDR_GOAL_SPEED_L) &&
        (end >= (uint16_t)(FEETECH_STS_ADDR_GOAL_SPEED_L + 1U))) {
        offset = (uint16_t)FEETECH_STS_ADDR_GOAL_SPEED_L - start;
        out_target->speed = FeetechSts_MakeU16(data[offset], data[(uint8_t)(offset + 1U)]);
    }

    out_target->valid = 1U;
    return 1U;
}

// 写入范围覆盖目标位置 -> 控制命令。
static uint8_t FeetechStsCommand_WriteTouchesTarget(uint8_t address, uint8_t data_len)
{
    uint16_t start;
    uint16_t end;

    if (data_len == 0U) {
        return 0U;
    }

    start = address;
    end = (uint16_t)address + (uint16_t)data_len - 1U;

    if ((start <= (uint16_t)(FEETECH_STS_ADDR_GOAL_POSITION_L + 1U)) &&
        (end >= (uint16_t)FEETECH_STS_ADDR_GOAL_POSITION_L)) {
        return 1U;
    }

    return 0U;
}

// 判断是否需要等待应答。
static uint8_t FeetechStsCommand_ShouldWaitResponse(const uint8_t *frame, uint8_t len)
{
    uint8_t id;
    uint8_t instruction;

    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }

    id = frame[2];
    instruction = frame[4];

    if (id == FEETECH_STS_BROADCAST_ID) {                       // 广播帧通常不等待应答，避免多舵机同时回包
        if ((instruction == FEETECH_STS_INST_SYNC_READ) ||
            (instruction == FEETECH_STS_INST_PING)) {
            return 1U;
        }
        return 0U;
    }

    switch (instruction) {
        case FEETECH_STS_INST_PING:
        case FEETECH_STS_INST_READ:
        case FEETECH_STS_INST_SYNC_READ:
            return 1U;

#if (FEETECH_STS_COMMAND_FORWARD_WRITE_WAIT_RESP != 0U)
        case FEETECH_STS_INST_WRITE:
        case FEETECH_STS_INST_REG_WRITE:
        case FEETECH_STS_INST_RECOVERY:
        case FEETECH_STS_INST_RESET:
            return 1U;
#endif

        default:
            return 0U;
    }
}
/*============================================================
 * 8. 官方寄存器读写辅助
 *============================================================*/

// 校验普通写、异步写和同步写的连续寄存器范围。
static uint8_t FeetechStsCommand_ValidateWriteFrame(const uint8_t *frame, uint8_t len, uint8_t source)
{
#if (CLAW_FEATURE_STS_REGISTER_GUARD_ENABLE != 0U)
    const FeetechStsRegisterDesc_t *failed_desc;
    FeetechStsRegisterCheckResult_t result;
    uint8_t instruction;
    uint8_t address;
    uint8_t data_len;
    uint8_t param_len;
    uint8_t item_len;
    uint8_t record_len;
    uint8_t payload_len;
    uint8_t offset;
    int32_t failed_value;

    if ((frame == 0) || (len < 6U)) {
        return 0U;
    }
    instruction = frame[4];
    if ((instruction == FEETECH_STS_INST_WRITE) ||
        (instruction == FEETECH_STS_INST_REG_WRITE)) {
        if ((frame[3] < 3U) || (len < 7U)) {
            FeetechStsCommand_SendText(source, "拒绝写入：帧内没有有效数据");
            return 0U;
        }
        address = frame[5];
        data_len = (uint8_t)(frame[3] - 3U);
        failed_desc = 0;
        failed_value = 0;
        result = FeetechStsRegister_CheckWriteRange(address,
                                                     &frame[6],
                                                     data_len,
                                                     &failed_desc,
                                                     &failed_value);
        if (result != FEETECH_STS_REG_CHECK_OK) {
            FeetechStsCommand_PrintWriteReject(source,
                                                address,
                                                result,
                                                failed_desc,
                                                failed_value);
            return 0U;
        }
#if (CLAW_FEATURE_STS_UNKNOWN_WRITE_ENABLE != 0U)
        if (FeetechStsRegister_IsKnownRange(address, data_len) == 0U) {
            FeetechStsCommand_Printf(source,
                                     "警告：未知寄存器写入已透传 地址=0x%02X 长度=%u",
                                     address,
                                     data_len);
        }
#endif
        return 1U;
    }

    if (instruction != FEETECH_STS_INST_SYNC_WRITE) {
        return 1U;
    }
    if ((frame[3] < 6U) || (len < 10U)) {
        FeetechStsCommand_SendText(source, "拒绝写入：SYNC_WRITE格式错误");
        return 0U;
    }

    address = frame[5];
    item_len = frame[6];
    param_len = (uint8_t)(frame[3] - 2U);
    payload_len = (uint8_t)(param_len - 2U);
    record_len = (uint8_t)(item_len + 1U);
    if ((item_len == 0U) || (record_len == 0U) ||
        (payload_len == 0U) || ((payload_len % record_len) != 0U)) {
        FeetechStsCommand_SendText(source, "拒绝写入：SYNC_WRITE数据长度错误");
        return 0U;
    }

    offset = 7U;
    while (offset < (uint8_t)(5U + param_len)) {
        failed_desc = 0;
        failed_value = 0;
        result = FeetechStsRegister_CheckWriteRange(address,
                                                     &frame[(uint8_t)(offset + 1U)],
                                                     item_len,
                                                     &failed_desc,
                                                     &failed_value);
        if (result != FEETECH_STS_REG_CHECK_OK) {
            FeetechStsCommand_PrintWriteReject(source,
                                                address,
                                                result,
                                                failed_desc,
                                                failed_value);
            return 0U;
        }
        offset = (uint8_t)(offset + record_len);
    }
#if (CLAW_FEATURE_STS_UNKNOWN_WRITE_ENABLE != 0U)
    if (FeetechStsRegister_IsKnownRange(address, item_len) == 0U) {
        FeetechStsCommand_Printf(source,
                                 "警告：未知同步寄存器写入已透传 地址=0x%02X 长度=%u",
                                 address,
                                 item_len);
    }
#endif
    return 1U;
#else
    (void)frame;
    (void)len;
    (void)source;
    return 1U;
#endif
}

// 输出写入拒绝原因。
static void FeetechStsCommand_PrintWriteReject(uint8_t source,
                                               uint8_t address,
                                               FeetechStsRegisterCheckResult_t result,
                                               const FeetechStsRegisterDesc_t *desc,
                                               int32_t value)
{
    if ((result == FEETECH_STS_REG_CHECK_UNKNOWN) || (desc == 0)) {
        FeetechStsCommand_Printf(source, "拒绝写入 地址0x%02X：未定义或不可写", address);
        return;
    }

    switch (result) {
        case FEETECH_STS_REG_CHECK_PARTIAL:
            FeetechStsCommand_Printf(source,
                                     "拒绝写入 %s：必须完整写入%u字节",
                                     desc->name,
                                     desc->width);
            break;
        case FEETECH_STS_REG_CHECK_READ_ONLY:
            FeetechStsCommand_Printf(source, "拒绝写入 %s：只读寄存器", desc->name);
            break;
        case FEETECH_STS_REG_CHECK_DANGEROUS:
            FeetechStsCommand_Printf(source, "拒绝写入 %s：危险写入未开放", desc->name);
            break;
        case FEETECH_STS_REG_CHECK_DISABLED:
            FeetechStsCommand_Printf(source, "拒绝写入 %s：项目未开放", desc->name);
            break;
        case FEETECH_STS_REG_CHECK_RANGE:
            if (desc->address == 40U) {
                FeetechStsCommand_Printf(source,
                                         "拒绝写入 %s：%ld仅允许0、1、128",
                                         desc->name,
                                         (long)value);
            } else {
                FeetechStsCommand_Printf(source,
                                         "拒绝写入 %s：%ld超出范围%ld~%ld",
                                         desc->name,
                                         (long)value,
                                         (long)desc->min_value,
                                         (long)desc->max_value);
            }
            break;
        default:
            FeetechStsCommand_Printf(source, "拒绝写入 %s：参数无效", desc->name);
            break;
    }
}

// 解析本次 READ 状态包。
static uint8_t FeetechStsCommand_PrintReadResponse(uint8_t address,
                                                   uint8_t read_len,
                                                   const uint8_t *response,
                                                   uint8_t response_len,
                                                   uint8_t source)
{
    const FeetechStsRegisterDesc_t *desc;
    const uint8_t *data;
    uint8_t param_len;
    uint8_t offset;
    uint8_t raw_start;
    uint8_t current_address;

    if ((response == 0) || (response_len < 6U) || (response[3] < 2U) ||
        (response_len != (uint8_t)(response[3] + 4U))) {
        FeetechStsCommand_Printf(source, "READ回包长度错误 实际=%u", response_len);
        return 0U;
    }
    param_len = (uint8_t)(response[3] - 2U);
    if (param_len != read_len) {
        FeetechStsCommand_Printf(source,
                                 "READ长度不符 请求=%u 回包=%u",
                                 read_len,
                                 param_len);
        return 0U;
    }
    data = &response[5];
    if (((uint16_t)address + (uint16_t)param_len) > 256U) {
        FeetechStsCommand_PrintReadRaw(address, data, param_len, source);
        return 1U;
    }

#if (CLAW_FEATURE_STS_REGISTER_TABLE_ENABLE == 0U)
    FeetechStsCommand_PrintReadRaw(address, data, param_len, source);
    return 1U;
#else
    offset = 0U;
    while (offset < param_len) {
        current_address = (uint8_t)(address + offset);
        desc = FeetechStsRegister_FindByStart(current_address);
        if ((desc != 0) && ((uint16_t)offset + desc->width <= param_len)) {
            FeetechStsCommand_PrintRegisterRead(source, desc, &data[offset]);
            offset = (uint8_t)(offset + desc->width);
            continue;
        }

        raw_start = offset;
        offset++;
        while (offset < param_len) {
            current_address = (uint8_t)(address + offset);
            desc = FeetechStsRegister_FindByStart(current_address);
            if ((desc != 0) && ((uint16_t)offset + desc->width <= param_len)) {
                break;
            }
            offset++;
        }
        FeetechStsCommand_PrintReadRaw((uint8_t)(address + raw_start),
                                       &data[raw_start],
                                       (uint8_t)(offset - raw_start),
                                       source);
    }
    return 1U;
#endif
}

// 输出未知 READ 的真实字节。
static void FeetechStsCommand_PrintReadRaw(uint8_t address,
                                           const uint8_t *data,
                                           uint8_t data_len,
                                           uint8_t source)
{
    char text[FEETECH_STS_COMMAND_STATUS_TEXT_MAX];
    uint16_t used;
    uint8_t i;
    int written;

    if ((data == 0) || (data_len == 0U)) {
        return;
    }
    written = snprintf(text,
                       sizeof(text),
                       "读取 地址=0x%02X 长度=%u 数据=",
                       address,
                       data_len);
    if (written < 0) {
        return;
    }
    used = (uint16_t)written;
    for (i = 0U; (i < data_len) && (used < (uint16_t)(sizeof(text) - 4U)); i++) {
        written = snprintf(&text[used], sizeof(text) - used, "%02X%s", data[i], (i + 1U < data_len) ? " " : "");
        if (written <= 0) {
            break;
        }
        used = (uint16_t)(used + (uint16_t)written);
    }
    text[sizeof(text) - 1U] = '\0';
    FeetechStsCommand_SendText(source, text);
}

// 输出一个已知寄存器的名称、单位和原始值。
static void FeetechStsCommand_PrintRegisterRead(uint8_t source,
                                                const FeetechStsRegisterDesc_t *desc,
                                                const uint8_t *data)
{
    int32_t value;
    int32_t abs_value;
    int32_t scaled;

    if ((desc == 0) || (data == 0) ||
        (FeetechStsRegister_DecodeValue(desc, data, desc->width, &value) == 0U)) {
        return;
    }

    if ((desc->address == 42U) || (desc->address == 56U)) {
        FeetechStsCommand_Printf(source,
                                 "%s=%u度 原始=%ld",
                                 desc->name,
                                 (uint16_t)(FeetechSts_PositionToAngle((uint16_t)value) + 0.5f),
                                 (long)value);
        return;
    }
    if (desc->address == 18U) {
        FeetechStsCommand_Printf(source,
                                 "相位特殊字节=0x%02lX 电压采样=%s",
                                 (long)value,
                                 ((value & 0x20L) != 0L) ? "高压1K" : "低压1.5K");
        return;
    }
    if ((desc->address == 19U) || (desc->address == 20U)) {
        FeetechStsCommand_Printf(source,
                                 "%s=0x%02lX：负载%s 过流%s 过热%s 磁编码%s 电压%s",
                                 desc->name,
                                 (long)value,
                                 ((value & 0x20L) != 0L) ? "开" : "关",
                                 ((value & 0x08L) != 0L) ? "开" : "关",
                                 ((value & 0x04L) != 0L) ? "开" : "关",
                                 ((value & 0x02L) != 0L) ? "开" : "关",
                                 ((value & 0x01L) != 0L) ? "开" : "关");
        return;
    }
    if (desc->address == 65U) {
        FeetechStsCommand_PrintServoStatus(source, "舵机状态", (uint8_t)value);
        return;
    }

    switch (desc->unit) {
        case FEETECH_STS_REG_UNIT_STEP:
            FeetechStsCommand_Printf(source, "%s=%ld步 原始=%ld", desc->name, (long)value, (long)value);
            break;
        case FEETECH_STS_REG_UNIT_STEP_PER_S:
            FeetechStsCommand_Printf(source, "%s=%ld步/秒 原始=%ld", desc->name, (long)value, (long)value);
            break;
        case FEETECH_STS_REG_UNIT_VOLT_01V:
            FeetechStsCommand_Printf(source,
                                     "%s=%ld.%ldV 原始=%ld",
                                     desc->name,
                                     (long)(value / 10L),
                                     (long)(value % 10L),
                                     (long)value);
            break;
        case FEETECH_STS_REG_UNIT_CURRENT_65MA:
            abs_value = (value < 0L) ? -value : value;
            scaled = abs_value * 65L;
            FeetechStsCommand_Printf(source,
                                     "%s=%s%ld.%ldmA 原始=%ld",
                                     desc->name,
                                     (value < 0L) ? "-" : "",
                                     (long)(scaled / 10L),
                                     (long)(scaled % 10L),
                                     (long)value);
            break;
        case FEETECH_STS_REG_UNIT_CELSIUS:
            FeetechStsCommand_Printf(source, "%s=%ld℃ 原始=%ld", desc->name, (long)value, (long)value);
            break;
        case FEETECH_STS_REG_UNIT_PERCENT_001:
            abs_value = (value < 0L) ? -value : value;
            FeetechStsCommand_Printf(source,
                                     "%s=%s%ld.%ld%% 原始=%ld",
                                     desc->name,
                                     (value < 0L) ? "-" : "",
                                     (long)(abs_value / 10L),
                                     (long)(abs_value % 10L),
                                     (long)value);
            break;
        case FEETECH_STS_REG_UNIT_PERCENT_01:
            FeetechStsCommand_Printf(source, "%s=%ld%% 原始=%ld", desc->name, (long)value, (long)value);
            break;
        case FEETECH_STS_REG_UNIT_MS:
            FeetechStsCommand_Printf(source, "%s=%ldms 原始=%ld", desc->name, (long)value, (long)value);
            break;
        case FEETECH_STS_REG_UNIT_10MS:
            FeetechStsCommand_Printf(source, "%s=%ldms 原始=%ld", desc->name, (long)(value * 10L), (long)value);
            break;
        case FEETECH_STS_REG_UNIT_2US:
            FeetechStsCommand_Printf(source, "%s=%ldus 原始=%ld", desc->name, (long)(value * 2L), (long)value);
            break;
        case FEETECH_STS_REG_UNIT_ACC_100_STEP_PER_S2:
            FeetechStsCommand_Printf(source, "%s=%ld步/秒2 原始=%ld", desc->name, (long)(value * 100L), (long)value);
            break;
        default:
            if (desc->value_type == FEETECH_STS_REG_VALUE_BITFIELD) {
                FeetechStsCommand_Printf(source, "%s=0x%02lX", desc->name, (long)value);
            } else {
                FeetechStsCommand_Printf(source, "%s=%ld", desc->name, (long)value);
            }
            break;
    }
}

// 输出写入 ACK 对应的寄存器值。
static void FeetechStsCommand_PrintWriteSummary(const uint8_t *frame,uint8_t frame_len,uint8_t source,const char *prefix)
{
    const FeetechStsRegisterDesc_t *desc;
    char text[FEETECH_STS_COMMAND_STATUS_TEXT_MAX];
    uint8_t address;
    uint8_t data_len;
    uint8_t offset;
    uint16_t used;
    int32_t value;
    int written;

    if ((frame == 0) || (frame_len < 7U) || (frame[3] < 3U)) {
        FeetechStsCommand_SendText(source, "写入成功");
        return;
    }
    address = frame[5];
    data_len = (uint8_t)(frame[3] - 3U);
    used = (uint16_t)snprintf(text, sizeof(text), "%s",(prefix != 0) ? prefix : "写入");
    offset = 0U;
    while ((offset < data_len) && (used < (uint16_t)(sizeof(text) - 1U))) {
        desc = FeetechStsRegister_FindByStart((uint8_t)(address + offset));
        if ((desc == 0) || ((uint16_t)offset + desc->width > data_len) ||
            (FeetechStsRegister_DecodeValue(desc, &frame[(uint8_t)(6U + offset)], (uint8_t)(data_len - offset), &value) == 0U)) {
            offset++;
            continue;
        }
        if (desc->unit == FEETECH_STS_REG_UNIT_VOLT_01V) {
            written = snprintf(&text[used],
                               sizeof(text) - used,
                               " %s=%ld.%ldV",
                               desc->name,
                               (long)(value / 10L),
                               (long)(value % 10L));
        } else {
            written = snprintf(&text[used],
                               sizeof(text) - used,
                               " %s=%ld",
                               desc->name,
                               (long)value);
        }
        if (written <= 0) {
            break;
        }
        used = (uint16_t)(used + (uint16_t)written);
        offset = (uint8_t)(offset + desc->width);
    }
    text[sizeof(text) - 1U] = '\0';
    FeetechStsCommand_SendText(source, text);
}

// 输出舵机状态位，可同时显示多个告警。
static void FeetechStsCommand_PrintServoStatus(uint8_t source, const char *prefix, uint8_t error)
{
    char detail[96];

    if (error == 0U) {
        if ((prefix != 0) && (strcmp(prefix, "舵机状态") == 0)) {
            FeetechStsCommand_SendText(source, "舵机状态=0x00");
        } else {
            FeetechStsCommand_Printf(source, "%s 状态=0x00", (prefix != 0) ? prefix : "舵机");
        }
        return;
    }
    detail[0] = '\0';
    if ((error & 0x01U) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "电压异常");
    }
    if ((error & 0x02U) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "磁编码异常");
    }
    if ((error & 0x04U) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "温度过热");
    }
    if ((error & 0x08U) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "电流过流");
    }
    if ((error & 0x20U) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "负载过载");
    }
    if ((error & (uint8_t)~0x2FU) != 0U) {
        FeetechStsCommand_AppendStatusItem(detail, sizeof(detail), "其他告警");
    }
    if ((prefix != 0) && (strcmp(prefix, "舵机状态") == 0)) {
        FeetechStsCommand_Printf(source,
                                 "舵机状态=0x%02X：%s",
                                 error,
                                 (detail[0] != '\0') ? detail : "未知告警");
    } else {
        FeetechStsCommand_Printf(source,
                                 "%s 舵机告警=0x%02X：%s",
                                 (prefix != 0) ? prefix : "舵机",
                                 error,
                                 (detail[0] != '\0') ? detail : "未知告警");
    }
}

// 向告警说明追加一个短项。
static void FeetechStsCommand_AppendStatusItem(char *text, uint16_t text_size, const char *item)
{
    uint16_t used;

    if ((text == 0) || (item == 0) || (text_size == 0U)) {
        return;
    }
    used = (uint16_t)strlen(text);
    if ((used > 0U) && (used < (uint16_t)(text_size - 1U))) {
        strncat(text, "、", (size_t)(text_size - used - 1U));
        used = (uint16_t)strlen(text);
    }
    if (used < (uint16_t)(text_size - 1U)) {
        strncat(text, item, (size_t)(text_size - used - 1U));
    }
    text[text_size - 1U] = '\0';
}
