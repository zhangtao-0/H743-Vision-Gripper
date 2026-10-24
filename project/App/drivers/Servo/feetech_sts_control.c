/*********************************************************************************************************************
 * @file    feetech_sts_control.c
 * @brief   机械爪本地动作控制实现。
 *
 * @details
 * 1. 本文件负责：管理 A0 夹取/松开、B0 扫描、反馈识别和保护状态。
 * 2. 本文件不负责：解析底层协议字节流。
 * 3. 主要流程：按键/命令 -> 目标缓存 -> 总线下发 -> 反馈闭环。
 *********************************************************************************************************************/

#include "feetech_sts_control.h"
#include "claw_limit_storage.h"
#include "shared_state.h"
#include <string.h>
#include <limits.h>

/*============================================================
 * 1. 内部配置和变量
 *============================================================*/

static uint8_t s_open_close_closed;               /* 0：当前逻辑偏张开；1：当前逻辑偏闭合/夹持。 */
static FeetechStsSweepPrintEvent_t s_sweep_print_event; /* B0 扫描打印事件。 */
static uint8_t s_sweep_print_event_ready;         /* 1：有待打印的 B0 扫描事件。 */
static FeetechStsMotionPrintEvent_t s_motion_print_event; /* 到位/卡死打印事件。 */
static uint8_t s_motion_print_event_ready;         /* 1：有待打印的运动事件。 */


static uint8_t s_feedback_retry_active;            /* 1：反馈读取失败，按重试周期再次读取。 */

/* 自适应与保护变量：默认关闭，集中放置便于后续移除。 */
static const FeetechStsGripConfig_t s_grip_profiles[FEETECH_STS_GRIP_PROFILE_COUNT] = {
    { FEETECH_STS_CONTROL_A0_CLOSE_SPEED, 120U, 650U, 650U, 120U, 3U, 200U, 3000U, 5.0f },      /* SOFT：A0 闭合速度跟随 B0，阈值和转矩仍按档位区分。 */
    { FEETECH_STS_CONTROL_A0_CLOSE_SPEED, 220U, 1000U, 1000U, 180U, 3U, 200U, 3000U, 4.0f },    /* MEDIUM：中等硬度，速度跟随 B0，转矩和阈值保持中等档。 */
    { FEETECH_STS_CONTROL_A0_CLOSE_SPEED, 350U, 1500U, 1000U, 260U, 3U, 180U, 3000U, 4.0f },    /* HARD：硬物，速度跟随 B0，阈值更高。 */
    { FEETECH_STS_CONTROL_A0_CLOSE_SPEED, 80U, 450U, 450U, 90U, 4U, 250U, 3500U, 5.0f }         /* FRAGILE：易碎物体，速度跟随 B0，靠低转矩和低阈值保护。 */
};
static FeetechStsGripDetectEvent_t s_grip_detect_event; /* 夹取确认调试事件，供命令层取走打印。 */
static uint8_t s_grip_detect_event_ready;         /* 1：有待打印的夹取调试事件。 */
static volatile uint8_t s_adaptive_grip_enable = FEETECH_STS_CONTROL_ADAPTIVE_GRIP_ENABLE; /* 自适应运行开关。 */

/*============================================================
 * 2. 内部函数声明
 *============================================================*/

static void FeetechStsControl_SetState(FeetechStsControlState_t state);
static void FeetechStsControl_ClearMotionRuntime(uint8_t clear_print_events);
static HAL_StatusTypeDef FeetechStsControl_MoveToAngleWithSpeed(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc);
static HAL_StatusTypeDef FeetechStsControl_SendAngleToBackend(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc);
static HAL_StatusTypeDef FeetechStsControl_SendPositionToBackend(uint16_t position, float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc);

static float FeetechStsControl_LimitFloatRange(float value, float min_value, float max_value);
static void FeetechStsControl_TaskFeedback(uint32_t now);
static void FeetechStsControl_TaskStall(uint32_t now);
static void FeetechStsControl_StartMotionTrack(uint32_t now);
static void FeetechStsControl_RecordMotionPrintEvent(uint8_t stalled, const FeetechStsFeedback_t *feedback);
static void FeetechStsControl_FinishMotionReached(const FeetechStsFeedback_t *feedback,FeetechStsControlState_t finish_state);
static uint16_t FeetechStsControl_GetStopHoldPosition(const FeetechStsFeedback_t *feedback);
static HAL_StatusTypeDef FeetechStsControl_HoldStopPosition(const FeetechStsFeedback_t *feedback);
static void FeetechStsControl_UpdateLastMotionDirection(float current_angle, float target_angle);static void FeetechStsControl_TaskSweep(uint32_t now);
static HAL_StatusTypeDef FeetechStsControl_SweepStep(uint32_t now, const char *event_text);
static void FeetechStsControl_TaskRelease(uint32_t now);
static void FeetechStsControl_UpdatePresentMetric(void);
static uint16_t FeetechStsControl_AbsS16(int16_t value);
static uint16_t FeetechStsControl_LimitTorque(uint16_t value);
static float FeetechStsControl_LimitLocalAngle(float angle_deg);
static HAL_StatusTypeDef FeetechStsControl_SetTorqueLimitLocal(uint16_t limit);
static void FeetechStsControl_SetResult(FeetechStsGripResult_t result);
static uint8_t FeetechStsControl_IsHoldLikeState(void);
static void FeetechStsControl_InitManager(void);
static void FeetechStsControl_LoadDefaultParams(void);
static void FeetechStsControl_ApplyConfigToRuntime(void);
static int32_t FeetechStsControl_ParamLimit(FeetechStsParamId_t id, int32_t value);
static float FeetechStsControl_AbsFloatDiff(float a, float b);
static HAL_StatusTypeDef FeetechStsControl_RequestLocalOwner(FeetechStsControlOwner_t owner);
static uint8_t FeetechStsControl_CanLocalWriteTarget(void);
static uint8_t FeetechStsControl_CanClearFaultByKey(void);
static void FeetechStsControl_ClearSoftFault(void);
static void FeetechStsControl_RecordSweepPrintEvent(const char *text, float angle_deg, float target_angle_deg);
static void FeetechStsControl_LoadStoredLimits(void);
static uint8_t FeetechStsControl_AreLimitPositionsValid(uint16_t open_position, uint16_t close_position);
static HAL_StatusTypeDef FeetechStsControl_RecordLimit(uint8_t record_open);
static float FeetechStsControl_GetMotionOpenAngle(void);
static float FeetechStsControl_GetMotionCloseAngle(void);

// 自适应与保护声明：默认关闭，集中放置便于后续移除。
static void FeetechStsControl_TaskAdaptiveGrip(uint32_t now);
static void FeetechStsControl_TaskOverload(uint32_t now);
static void FeetechStsControl_TaskLowVoltage(uint32_t now);
static void FeetechStsControl_TaskSlipProtect(uint32_t now);
static uint8_t FeetechStsControl_IsNearCloseLimit(const FeetechStsFeedback_t *feedback, const FeetechStsGripConfig_t *cfg);
static void FeetechStsControl_HandleNoObject(FeetechStsGripResult_t result);
static void FeetechStsControl_HandleGripSuccess(void);
static uint8_t FeetechStsControl_IsLowVoltageActive(void);
static void FeetechStsControl_ResetGripStableState(const FeetechStsFeedback_t *feedback);
static uint8_t FeetechStsControl_IsGripAngleStable(const FeetechStsFeedback_t *feedback);
static void FeetechStsControl_RecordGripDetectEvent(const char *reason, const FeetechStsFeedback_t *feedback, uint8_t confirm_count);
static uint8_t FeetechStsControl_ShouldRunMotionTask(void);

// 初始化控制层：加载配置和机械极限，上电不下发运动目标。
void FeetechStsControl_Init(void)
{
    ClawSharedState_Reset();
    FeetechStsControl_InitManager();
    FeetechStsControl_LoadStoredLimits();
    g_claw_control_state.sweep_range = (FeetechStsSweepRange_t)g_claw_feature_defaults.motion_range;
    if (g_claw_control_state.sweep_range > FEETECH_STS_SWEEP_RANGE_FULL_270) {
        g_claw_control_state.sweep_range = FEETECH_STS_SWEEP_RANGE_FULL_270;
    }
    if ((g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_LIMITS) &&
        (g_claw_control_state.limits_valid == 0U)) {
        g_claw_control_state.sweep_range = FEETECH_STS_SWEEP_RANGE_FULL_270;
    }

    g_claw_control_state.state = FEETECH_STS_CONTROL_IDLE;
    g_claw_control_state.profile = FEETECH_STS_GRIP_PROFILE_MEDIUM;
    g_claw_control_state.result = FEETECH_STS_GRIP_RESULT_NONE;
    g_claw_control_state.servo_id = g_claw_manager_state.config.servo_id;
    g_claw_control_state.object_detected = 0U;
    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.sweep_direction_close = 1U;
    g_claw_control_state.last_motion_direction_close = 1U;
    g_claw_control_state.grip_trigger_counter = 0U;
    g_claw_control_state.overload_active = 0U;
    g_claw_control_state.limit_setting_active = 0U;
    g_claw_control_state.pending_open_valid = 0U;
    g_claw_control_state.pending_close_valid = 0U;
    g_claw_control_state.target_angle_deg = g_claw_manager_state.config.open_angle_deg;
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(g_claw_control_state.target_angle_deg);
    g_claw_control_state.target_time_ms = g_claw_manager_state.config.default_move_time_ms;
    g_claw_control_state.target_speed = g_claw_manager_state.config.default_speed;
    g_claw_control_state.target_torque_limit = g_claw_manager_state.config.full_torque_limit;
    g_claw_control_state.present_load_abs = 0U;
    g_claw_control_state.present_current_ma = 0U;
    g_claw_control_state.hold_position = g_claw_control_state.target_position;
    g_claw_control_state.hold_angle_deg = g_claw_control_state.target_angle_deg;
    g_claw_control_state.hold_load_abs = 0U;
    g_claw_control_state.low_voltage_active = 0U;
    g_claw_control_state.release_permission = 1U;
    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.slip_compensate_count = 0U;
    g_claw_control_state.owner = FEETECH_STS_OWNER_NONE;
    g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_AUTO;
    g_claw_control_state.fault_level = FEETECH_STS_FAULT_LEVEL_NONE;
    g_claw_control_state.local_write_enable = 0U;        // 首条按键动作前禁止周期下发，保证复位静止
    g_claw_control_state.last_external_write_tick_ms = 0U;
    g_claw_control_state.owner_timeout_ms = FEETECH_STS_CONTROL_EXTERNAL_TIMEOUT_MS;
    g_claw_control_state.grip_last_angle_deg = 0.0f;
    g_claw_control_state.grip_delta_last_deg = 0.0f;
    g_claw_control_state.last_status = HAL_OK;
    g_claw_control_state.state_tick_ms = HAL_GetTick();
    g_claw_control_state.last_feedback_tick_ms = 0U;
    g_claw_control_state.overload_start_tick_ms = 0U;
    g_claw_control_state.last_slip_compensate_tick_ms = 0U;
    g_claw_control_state.grip_last_rx_packet_count = 0UL;
    s_open_close_closed = 0U;
    g_claw_control_state.motion_track_active = 0U;
    g_claw_control_state.stall_reference_position = 0U;
    g_claw_control_state.motion_start_tick_ms = 0UL;
    g_claw_control_state.stall_reference_tick_ms = 0UL;
    g_claw_control_state.stall_last_feedback_tick_ms = 0UL;
    s_motion_print_event_ready = 0U;
    s_feedback_retry_active = 0U;

    FeetechStsControl_SyncManager();
}

// 调度反馈、夹取、释放、扫描和保护任务。
void FeetechStsControl_Task(void)
{
    // 主入口：读反馈 -> 安全/状态机 -> 本地目标发送 -> 管理快照。
    uint32_t now;

    now = HAL_GetTick();
    // 反馈先刷新，状态机使用最新缓存。
    FeetechStsControl_TaskFeedback(now);
    FeetechStsControl_UpdatePresentMetric();
    FeetechStsControl_TaskStall(now);
    //FeetechStsControl_TaskLowVoltage(now);
    //FeetechStsControl_TaskOverload(now);
    // 按键卡顿观察点：A0 自适应分析集中在此入口。
    FeetechStsControl_TaskAdaptiveGrip(now);
    if (FEETECH_STS_CONTROL_SLIP_PROTECT_ENABLE != 0U) {
        FeetechStsControl_TaskSlipProtect(now);
    }
    FeetechStsControl_TaskRelease(now);
    FeetechStsControl_TaskSweep(now);

    if (FeetechStsControl_ShouldRunMotionTask() != 0U) {
        FeetechStsMotion_Task();
    }

    FeetechStsControl_SyncManager();
}

/*============================================================
 * 4. 配置和基础动作接口
 *============================================================*/

// 配置 STS 舵机基础模式和力矩。
HAL_StatusTypeDef FeetechStsControl_ConfigureServo(void)
{
    HAL_StatusTypeDef status;

    // 卸力 -> 写当前位置目标 -> 限矩 -> 上力，避免沿用旧目标跳动。
    status = FeetechStsBus_DisableTorque(g_claw_control_state.servo_id);
    if (status == HAL_OK) {
        status = FeetechStsBus_SetMode(g_claw_control_state.servo_id, FEETECH_STS_MODE_POSITION);
    }
    if (status == HAL_OK) {
        status = FeetechStsBus_SetPositionEx(g_claw_control_state.servo_id,
                                             g_claw_control_state.target_position,
                                             0U,
                                             0U,
                                             g_claw_manager_state.config.default_acc);
    }
    if (status == HAL_OK) {
        status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
    }
    if (status == HAL_OK) {
        status = FeetechStsBus_EnableTorque(g_claw_control_state.servo_id);
    }
    if (status != HAL_OK) {
        g_claw_control_state.last_status = status;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        return status;
    }

    g_claw_control_state.last_status = HAL_OK;
    return HAL_OK;
}


// 按默认速度移动到目标角度。
HAL_StatusTypeDef FeetechStsControl_MoveToAngle(float angle_deg, uint16_t time_ms)
{
    // 使用默认速度/加速度设置本地角度目标。
    return FeetechStsControl_MoveToAngleWithSpeed(angle_deg,
                                                  time_ms,
                                                  g_claw_manager_state.config.default_speed,
                                                  g_claw_manager_state.config.default_acc);
}

// 按指定时间/速度移动到目标角度。
static HAL_StatusTypeDef FeetechStsControl_MoveToAngleWithSpeed(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    // 目标缓存：限幅角度 -> motion 目标 -> 总线写入。
    float limited_angle;

    // 当前来源无本机写权 -> 不写目标。
    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        g_claw_control_state.last_status = HAL_BUSY;
        return HAL_BUSY;
    }

    limited_angle = FeetechStsControl_LimitLocalAngle(angle_deg);
    if ((time_ms == 0U) && (speed == 0U)) {
        time_ms = g_claw_manager_state.config.default_move_time_ms;
    }

    g_claw_control_state.target_angle_deg = limited_angle;
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(limited_angle);
    g_claw_control_state.target_time_ms = time_ms;
    g_claw_control_state.target_speed = speed;

    FeetechStsMotion_SetTargetEx(limited_angle,
                                 time_ms,
                                 speed,
                                 acc);

    g_claw_control_state.last_status = FeetechStsControl_SendAngleToBackend(limited_angle,
                                                                       time_ms,
                                                                       speed,
                                                                       acc);
    if (g_claw_control_state.last_status == HAL_OK) {
        FeetechStsControl_UpdateLastMotionDirection(FeetechStsControl_GetActualAngle(), limited_angle);
        FeetechStsControl_StartMotionTrack(HAL_GetTick());        // 新目标重新武装到位/卡死判断
    }
    if (g_claw_control_state.last_status != HAL_OK) {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
    }

    return g_claw_control_state.last_status;
}
// 执行松开动作。
HAL_StatusTypeDef FeetechStsControl_Open(void)
{
    // A0 松开入口：请求控制权后执行张开动作。
    return FeetechStsControl_Release();
}

// 释放别名，执行松开动作。
HAL_StatusTypeDef FeetechStsControl_Release(void)
{
    // 确认释放入口：解除夹持并回到张开方向。
    HAL_StatusTypeDef status;

    status = FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_A0);
    if (status != HAL_OK) {
        return status;
    }

    if ((FeetechStsControl_IsMotionRangeReady() == 0U) || (g_claw_control_state.limit_setting_active != 0U)) {
        return HAL_ERROR;
    }

    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.motion_track_active = 0U;        // 手动停止后不再判断卡死
    g_claw_control_state.object_detected = 0U;
    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
    g_claw_control_state.overload_active = 0U;
    s_open_close_closed = 0U;
    g_claw_control_state.last_motion_direction_close = 0U;

    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
    if (status != HAL_OK) {
        g_claw_control_state.last_status = status;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        return status;
    }

    FeetechStsControl_SetState(FEETECH_STS_CONTROL_RELEASING);
    g_claw_control_state.target_speed = g_claw_manager_state.config.open_speed;
    return FeetechStsControl_MoveToAngleWithSpeed(FeetechStsControl_GetMotionOpenAngle(),
                                                  g_claw_manager_state.config.default_release_time_ms,
                                                  g_claw_manager_state.config.open_speed,
                                                  g_claw_manager_state.config.default_acc);
}

// 执行普通闭合动作。
HAL_StatusTypeDef FeetechStsControl_Close(void)
{
    // 直接闭合入口：使用普通目标角闭合。
    return FeetechStsControl_StartGrip();
}

// 启动 A0 自适应夹取。
HAL_StatusTypeDef FeetechStsControl_StartGrip(void)
{
    // A0 夹取入口：固定速度闭合，反馈只用于夹取判断。
    const FeetechStsFeedback_t *feedback;
    HAL_StatusTypeDef status;

    status = FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_A0);
    if (status != HAL_OK) {
        return status;
    }

    if ((FeetechStsControl_IsMotionRangeReady() == 0U) || (g_claw_control_state.limit_setting_active != 0U)) {
        return HAL_ERROR;
    }

    if (FeetechStsControl_IsLowVoltageActive() != 0U) {
        g_claw_control_state.low_voltage_active = 1U;
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE);
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_FAULT_HOLD);
        return HAL_ERROR;
    }

    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.motion_track_active = 0U;        // 手动停止后不再判断卡死
    g_claw_control_state.object_detected = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
    g_claw_control_state.overload_active = 0U;
    g_claw_control_state.low_voltage_active = 0U;
    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.slip_compensate_count = 0U;
    g_claw_control_state.hold_position = 0U;
    g_claw_control_state.hold_angle_deg = 0.0f;
    g_claw_control_state.hold_load_abs = 0U;
    g_claw_control_state.last_slip_compensate_tick_ms = 0U;
    g_claw_control_state.grip_last_rx_packet_count = 0UL;

    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        g_claw_control_state.last_status = HAL_BUSY;
        return HAL_BUSY;
    }

    feedback = FeetechStsBus_GetFeedback();
    if (feedback != 0) {
        g_claw_control_state.grip_last_rx_packet_count = feedback->rx_packet_count;
    }
    FeetechStsControl_ResetGripStableState(feedback);
    s_open_close_closed = 1U;
    g_claw_control_state.last_motion_direction_close = 1U;
    FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);

#if (FEETECH_STS_CONTROL_ADAPTIVE_GRIP_ENABLE != 0U)
    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.approach_torque_limit);
#else
    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
#endif
    if (status != HAL_OK) {
        g_claw_control_state.last_status = status;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        return status;
    }

    g_claw_control_state.target_angle_deg = FeetechStsControl_GetMotionCloseAngle();
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(g_claw_control_state.target_angle_deg);
    g_claw_control_state.target_time_ms = g_claw_manager_state.config.default_grip_time_ms;
    g_claw_control_state.target_speed = g_claw_manager_state.config.close_speed;

    FeetechStsMotion_SetTargetEx(g_claw_control_state.target_angle_deg,
                                 g_claw_control_state.target_time_ms,
                                 g_claw_control_state.target_speed,
                                 g_claw_manager_state.config.default_acc);

    FeetechStsControl_SetState(FEETECH_STS_CONTROL_CLOSING);
    g_claw_control_state.last_status = FeetechStsControl_SendAngleToBackend(g_claw_control_state.target_angle_deg,
                                                                       g_claw_control_state.target_time_ms,
                                                                       g_claw_control_state.target_speed,
                                                                       g_claw_manager_state.config.default_acc);
    if (g_claw_control_state.last_status == HAL_OK) {
        FeetechStsControl_StartMotionTrack(HAL_GetTick());        // A0闭合重新武装到位/卡死判断
    }    if (g_claw_control_state.last_status != HAL_OK) {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
    }

    return g_claw_control_state.last_status;
}

// 切换开合目标。
HAL_StatusTypeDef FeetechStsControl_ToggleOpenClose(void)
{
    // 按当前开合标志在张开和夹取之间切换。
    if ((g_claw_control_state.state == FEETECH_STS_CONTROL_CLOSING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_CONTACT) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_SETTLE) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_LOCKED) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_CARRY_HOLD) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_HOLDING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_FAULT_HOLD) ||
        (s_open_close_closed != 0U)) {
        return FeetechStsControl_ConfirmRelease();
    }

    return FeetechStsControl_StartGrip();
}

// 启动 B0 往复扫描。
HAL_StatusTypeDef FeetechStsControl_StartSweep(void)
{
    // B0 扫描入口：从当前真实角度沿最近有效方向开始往复扫描。
    const FeetechStsFeedback_t *feedback;
    HAL_StatusTypeDef status;
    uint32_t now;

    status = FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_B0);
    if (status != HAL_OK) {
        return status;
    }
    status = FeetechStsControl_RequestFeedback();
    if (status != HAL_OK) {
        return status;
    }
    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
    if (status != HAL_OK) {
        g_claw_control_state.last_status = status;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        return status;
    }

    now = HAL_GetTick();
    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U) &&
        ((uint32_t)(now - feedback->last_update_tick) <= FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS)) {
        g_claw_control_state.target_angle_deg = FeetechStsControl_LimitLocalAngle(feedback->angle_deg);
        g_claw_control_state.target_position = feedback->position;
        g_claw_control_state.grip_last_rx_packet_count = feedback->rx_packet_count;
    } else {
        g_claw_control_state.last_status = HAL_TIMEOUT;
        return HAL_TIMEOUT;
    }
    FeetechStsControl_ResetGripStableState(feedback);

    g_claw_control_state.sweep_active = 1U;
    g_claw_control_state.sweep_direction_close = g_claw_control_state.last_motion_direction_close;
    g_claw_control_state.object_detected = 0U;
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_SWEEP);

    return FeetechStsControl_SweepStep(now, "B0开始扫描");
}
// 停止扫描并同步当前位置。
HAL_StatusTypeDef FeetechStsControl_StopSweep(void)
{
    // B0 停止也是控制命令，可覆盖旧来源。
    HAL_StatusTypeDef status;

    status = FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_B0);
    if (status != HAL_OK) {
        return status;
    }

    return FeetechStsControl_StopMotion();
}

// 进入等待确认释放状态。
HAL_StatusTypeDef FeetechStsControl_ArmRelease(void)
{
    // 预置释放许可，等待下一步确认释放。
    if (g_claw_control_state.release_permission == 0U) {
        return HAL_ERROR;
    }

    g_claw_control_state.release_armed = 1U;
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_RELEASE_ARMED);
    return HAL_OK;
}

// 确认并执行释放动作。
HAL_StatusTypeDef FeetechStsControl_ConfirmRelease(void)
{
    // 释放确认：许可有效 -> 执行 Release。
    if (g_claw_control_state.release_permission == 0U) {
        return HAL_ERROR;
    }

    g_claw_control_state.release_armed = 0U;
    return FeetechStsControl_Release();
}

// 进入携带保持状态。
HAL_StatusTypeDef FeetechStsControl_SetCarryHold(void)
{
    // 进入携带保持状态，维持已夹持目标。
    if ((g_claw_control_state.object_detected == 0U) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_GRIP_LOCKED) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_HOLDING)) {
        return HAL_ERROR;
    }

    FeetechStsControl_SetState(FEETECH_STS_CONTROL_CARRY_HOLD);
    return HAL_OK;
}

// 读取当前位置并下发保持目标。
HAL_StatusTypeDef FeetechStsControl_HoldCurrent(void)
{
    // 把当前反馈角度作为保持目标；无反馈时使用目标缓存。
    const FeetechStsFeedback_t *feedback;
    uint16_t hold_position;
    float hold_angle;

    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        g_claw_control_state.last_status = HAL_BUSY;
        return HAL_BUSY;
    }

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U)) {
        hold_position = feedback->position;
        hold_angle = feedback->angle_deg;
    } else {
        hold_position = g_claw_control_state.target_position;
        hold_angle = g_claw_control_state.target_angle_deg;
    }

    FeetechStsControl_ClearMotionRuntime(0U);
    g_claw_control_state.hold_position = hold_position;
    g_claw_control_state.hold_angle_deg = hold_angle;
    g_claw_control_state.hold_load_abs = g_claw_control_state.present_load_abs;
    g_claw_control_state.target_position = hold_position;
    g_claw_control_state.target_angle_deg = hold_angle;
    g_claw_control_state.target_time_ms = g_claw_manager_state.config.default_move_time_ms;
    g_claw_control_state.target_speed = 0U;

    FeetechStsMotion_SetTargetEx(hold_angle,
                                 g_claw_manager_state.config.default_move_time_ms,
                                 0U,
                                 g_claw_manager_state.config.default_acc);

    g_claw_control_state.last_status = FeetechStsControl_SendPositionToBackend(hold_position,
                                                                          hold_angle,
                                                                          g_claw_manager_state.config.default_move_time_ms,
                                                                          0U,
                                                                          g_claw_manager_state.config.default_acc);
    if (g_claw_control_state.last_status == HAL_OK) {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
    } else {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
    }

    return g_claw_control_state.last_status;
}
// 手动停止，当前位置同步为目标。
HAL_StatusTypeDef FeetechStsControl_StopMotion(void)
{
      /*
     * 先停止旧扫描和旧目标维护，
     * 避免等待停止反馈期间再次发送原端点目标。
     */
    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.motion_track_active = 0U;
    
    // 手动停止：读取真实编码位置 -> 写回同一位置 -> 保持。
    const FeetechStsFeedback_t *feedback;
    HAL_StatusTypeDef feedback_status;
    uint32_t now;
    uint16_t stop_position;
    float stop_angle;

    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        (void)FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_A0);
    }

    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        g_claw_control_state.last_status = HAL_BUSY;
        return HAL_BUSY;
    }

    now = HAL_GetTick();
    feedback = FeetechStsBus_GetFeedback();
    feedback_status = HAL_OK;

    feedback_status = FeetechStsControl_RequestFeedback();
    now = HAL_GetTick();
    feedback = FeetechStsBus_GetFeedback();
    if ((feedback == 0) || (feedback->position_valid == 0U) ||
        ((feedback_status != HAL_OK) &&
         ((uint32_t)(now - feedback->last_update_tick) >
          FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS))) {

        FeetechStsControl_ClearMotionRuntime(1U);
        g_claw_control_state.local_write_enable = 0U;
        g_claw_control_state.last_status =
            (feedback_status == HAL_OK) ? HAL_ERROR : feedback_status;
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        return g_claw_control_state.last_status;
    }
    stop_position = feedback->position;
    stop_angle = feedback->angle_deg;

    FeetechStsControl_ClearMotionRuntime(1U);
    g_claw_control_state.object_detected = 0U;
    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
    g_claw_control_state.overload_active = 0U;
    g_claw_control_state.slip_compensate_count = 0U;
    s_open_close_closed = 0U;

    g_claw_control_state.hold_position = stop_position;
    g_claw_control_state.hold_angle_deg = stop_angle;
    g_claw_control_state.hold_load_abs = 0U;
    g_claw_control_state.target_position = stop_position;
    g_claw_control_state.target_angle_deg = stop_angle;
    g_claw_control_state.target_time_ms = 0U;
    g_claw_control_state.target_speed = 0U;

    FeetechStsMotion_SetTargetEx(stop_angle,
                                 0U,
                                 0U,
                                 g_claw_manager_state.config.default_acc);

    g_claw_control_state.last_status = FeetechStsControl_SendPositionToBackend(stop_position,
                                                                               stop_angle,
                                                                               0U,
                                                                               0U,
                                                                               g_claw_manager_state.config.default_acc);
    if (g_claw_control_state.last_status == HAL_OK) {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
    } else {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
    }

    return g_claw_control_state.last_status;
}
// 请求一次反馈并同步管理缓存。
HAL_StatusTypeDef FeetechStsControl_RequestFeedback(void)
{
    /*
     * 屏幕 A/I/U/L 长期不变时，常见原因是周期 READ 只发送不等待，随后动作写帧打断回读解析。
     * 这里等待完整应答窗口，确保新反馈先进入 FeetechStsBus 缓存，再供屏幕和状态机读取。
     */

    g_claw_control_state.last_status = FeetechStsBus_ReadFeedbackBlocking(g_claw_control_state.servo_id, FEETECH_STS_RESP_TIMEOUT_MS);
    return g_claw_control_state.last_status;
}
// 进入机械极限标定。
HAL_StatusTypeDef FeetechStsControl_BeginLimitSetting(void)
{
    if (FeetechStsControl_IsMotionState() != 0U) {
        if (FeetechStsControl_StopMotion() != HAL_OK) {
            return HAL_ERROR;
        }
    }

    g_claw_control_state.limit_setting_active = 1U;
    if (g_claw_control_state.limits_valid != 0U) {
        g_claw_control_state.pending_open_position = FeetechSts_AngleToPosition(g_claw_manager_state.config.open_angle_deg);
        g_claw_control_state.pending_close_position = FeetechSts_AngleToPosition(g_claw_manager_state.config.close_angle_deg);
        g_claw_control_state.pending_open_valid = 1U;
        g_claw_control_state.pending_close_valid = 1U;
    } else {
        g_claw_control_state.pending_open_valid = 0U;
        g_claw_control_state.pending_close_valid = 0U;
    }
    g_claw_control_state.sweep_range = FEETECH_STS_SWEEP_RANGE_FULL_270;
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_LIMIT_SETTING);
    return HAL_OK;
}

// 取消本轮标定，保留原持久化极限。
HAL_StatusTypeDef FeetechStsControl_CancelLimitSetting(void)
{
    if (g_claw_control_state.sweep_active != 0U) {
        (void)FeetechStsControl_StopMotion();
    }
    g_claw_control_state.limit_setting_active = 0U;
    g_claw_control_state.pending_open_valid = 0U;
    g_claw_control_state.pending_close_valid = 0U;
    g_claw_control_state.sweep_range = (g_claw_control_state.limits_valid != 0U) ? FEETECH_STS_SWEEP_RANGE_LIMITS : FEETECH_STS_SWEEP_RANGE_FULL_270;
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
    return HAL_OK;
}

// 校验并保存本轮机械极限。
HAL_StatusTypeDef FeetechStsControl_FinishLimitSetting(void)
{
    ClawLimitStorageData_t data;
    HAL_StatusTypeDef status;

    if ((g_claw_control_state.limit_setting_active == 0U) ||
        (g_claw_control_state.pending_open_valid == 0U) ||
        (g_claw_control_state.pending_close_valid == 0U)) {
        return HAL_ERROR;
    }
    if (g_claw_control_state.sweep_active != 0U) {
        (void)FeetechStsControl_StopMotion();
    }
    if (FeetechStsControl_AreLimitPositionsValid(g_claw_control_state.pending_open_position,
                                                  g_claw_control_state.pending_close_position) == 0U) {
        return HAL_ERROR;
    }

    status = ClawLimitStorage_SaveConfig(g_claw_control_state.pending_open_position,
                                         g_claw_control_state.pending_close_position,
                                         1U,
                                         (uint8_t)FEETECH_STS_SWEEP_RANGE_LIMITS);
    if (status != HAL_OK) {
        return status;
    }

    g_claw_manager_state.config.open_angle_deg = FeetechSts_PositionToAngle(g_claw_control_state.pending_open_position);
    g_claw_manager_state.config.close_angle_deg = FeetechSts_PositionToAngle(g_claw_control_state.pending_close_position);
    g_claw_control_state.limits_valid = 1U;
    g_claw_control_state.limit_setting_active = 0U;
    g_claw_control_state.pending_open_valid = 0U;
    g_claw_control_state.pending_close_valid = 0U;
    g_claw_control_state.sweep_range = FEETECH_STS_SWEEP_RANGE_LIMITS;
    if (ClawLimitStorage_Load(&data) == HAL_OK) {
        g_claw_control_state.stored_limit_sequence = data.sequence;
    }
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
    FeetechStsControl_SyncManager();
    return HAL_OK;
}
// 记录闭合机械极限。
HAL_StatusTypeDef FeetechStsControl_RecordCloseLimit(void)
{
    return FeetechStsControl_RecordLimit(0U);
}

// 记录张开机械极限。
HAL_StatusTypeDef FeetechStsControl_RecordOpenLimit(void)
{
    return FeetechStsControl_RecordLimit(1U);
}

// 清除持久化机械极限。
HAL_StatusTypeDef FeetechStsControl_ClearStoredLimits(void)
{
    HAL_StatusTypeDef status;

    if (FeetechStsControl_IsMotionState() != 0U) {
        return HAL_BUSY;
    }
    status = ClawLimitStorage_Clear();
    if (status == HAL_OK) {
        g_claw_control_state.limits_valid = 0U;
        g_claw_control_state.stored_limit_sequence = 0UL;
        g_claw_control_state.sweep_range = FEETECH_STS_SWEEP_RANGE_FULL_270;
    }
    return status;
}

// 读取机械极限有效性。
uint8_t FeetechStsControl_AreLimitsValid(void)
{
    return g_claw_control_state.limits_valid;
}

// 读取标定状态。
uint8_t FeetechStsControl_IsLimitSetting(void)
{
    return g_claw_control_state.limit_setting_active;
}

// 设置 B0 扫描范围。
HAL_StatusTypeDef FeetechStsControl_SetSweepRange(FeetechStsSweepRange_t range)
{
    if (range > FEETECH_STS_SWEEP_RANGE_FULL_270) {
        return HAL_ERROR;
    }
    if ((range == FEETECH_STS_SWEEP_RANGE_LIMITS) && (g_claw_control_state.limits_valid == 0U)) {
        return HAL_ERROR;
    }
    g_claw_control_state.sweep_range = range;
    return HAL_OK;
}

// 保存 A0/B0/滑块共同动作范围。
HAL_StatusTypeDef FeetechStsControl_SaveSweepRange(FeetechStsSweepRange_t range)
{
    uint16_t open_position;
    uint16_t close_position;
    HAL_StatusTypeDef status;

    status = FeetechStsControl_SetSweepRange(range);
    if (status != HAL_OK) {
        return status;
    }
    if (g_claw_control_state.limits_valid != 0U) {
        open_position = FeetechSts_AngleToPosition(g_claw_manager_state.config.open_angle_deg);
        close_position = FeetechSts_AngleToPosition(g_claw_manager_state.config.close_angle_deg);
    } else {
        open_position = 0U;
        close_position = 0U;
    }
    status = ClawLimitStorage_SaveConfig(open_position,
                                         close_position,
                                         g_claw_control_state.limits_valid,
                                         (uint8_t)range);
    return status;
}
// 读取 B0 扫描范围。
FeetechStsSweepRange_t FeetechStsControl_GetSweepRange(void)
{
    return g_claw_control_state.sweep_range;
}

// 判断 A0/B0/滑块共同动作范围是否可用。
uint8_t FeetechStsControl_IsMotionRangeReady(void)
{
    if (g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_FULL_270) {
        return 1U;
    }
    return g_claw_control_state.limits_valid;
}

// B0 立即反向扫描；未扫描时从当前动作的反方向接管。
HAL_StatusTypeDef FeetechStsControl_ReverseSweep(void)
{
    const FeetechStsFeedback_t *feedback;
    HAL_StatusTypeDef status;

    status = FeetechStsControl_RequestFeedback();
    if (status != HAL_OK) {
        return status;
    }

    if (g_claw_control_state.sweep_active == 0U) {
        status = FeetechStsControl_RequestLocalOwner(FEETECH_STS_OWNER_KEY_B0);
        if (status != HAL_OK) {
            return status;
        }
        status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
        if (status != HAL_OK) {
            return status;
        }
        g_claw_control_state.sweep_direction_close =
            (g_claw_control_state.last_motion_direction_close != 0U) ? 0U : 1U;

        feedback = FeetechStsBus_GetFeedback();
        if (feedback != 0) {
            g_claw_control_state.grip_last_rx_packet_count = feedback->rx_packet_count;
        }
        FeetechStsControl_ResetGripStableState(feedback);
        g_claw_control_state.sweep_active = 1U;
        g_claw_control_state.object_detected = 0U;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_SWEEP);
    }

    return FeetechStsControl_SweepStep(HAL_GetTick(), "B0换向");
}
// 判断当前是否存在物理运动。
uint8_t FeetechStsControl_IsMotionState(void)
{
    const FeetechStsFeedback_t *feedback;

    if (g_claw_control_state.motion_track_active != 0U) {        // 外部滑块目标仍在执行
        return 1U;
    }
    if ((g_claw_control_state.state == FEETECH_STS_CONTROL_OPENING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_CLOSING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_CONTACT) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_SETTLE) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_RELEASING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_SWEEP)) {
        return 1U;
    }

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U)) {
        if (feedback->moving != 0U) {
            return 1U;
        }
        if ((g_claw_control_state.control_mode == FEETECH_STS_CONTROL_MODE_MANUAL) &&
            (FeetechStsControl_AbsFloatDiff(feedback->angle_deg, g_claw_control_state.target_angle_deg) > 1.0f)) {
            return 1U;
        }
    }
    return 0U;
}


/*============================================================
 * 5. 状态读取接口
 *============================================================*/

// 读取控制状态缓存。
const FeetechStsControlStatus_t *FeetechStsControl_GetStatus(void)
{
    return &g_claw_control_state;
}

// 读取管理器快照。
const FeetechStsManager_t *FeetechStsControl_GetManager(void)
{
    FeetechStsControl_SyncManager();
    return &g_claw_manager_state;
}

// 设置状态打印开关。
void FeetechStsControl_SetPrintEnable(uint8_t enable)
{
    // 设置状态打印开关。
    g_claw_manager_state.option.print_enable = (enable == 0U) ? 0U : 1U;
}

// 读取状态打印开关。
uint8_t FeetechStsControl_GetPrintEnable(void)
{
    // 读取状态打印开关。
    return g_claw_manager_state.option.print_enable;
}

// 设置释放权限开关。
void FeetechStsControl_SetReleasePermission(uint8_t enable)
{
    // 设置是否允许执行释放动作。
    g_claw_control_state.release_permission = (enable != 0U) ? 1U : 0U;
    g_claw_manager_state.option.screen_update_request = 1U;
}

// 读取释放权限开关。
uint8_t FeetechStsControl_GetReleasePermission(void)
{
    // 读取释放许可标志。
    return g_claw_control_state.release_permission;
}

// 同步运行状态到管理器快照。
void FeetechStsControl_SyncManager(void)
{
    // 同步控制层、总线反馈和运动目标到管理结构。
    const FeetechStsBus_t *bus;
    const FeetechStsFeedback_t *feedback;
    const FeetechStsMotion_t *motion;

    bus = FeetechStsBus_GetBus();
    feedback = FeetechStsBus_GetFeedback();
    motion = FeetechStsBus_GetMotion();

    if ((bus == 0) || (feedback == 0) || (motion == 0)) {
        return;
    }

    g_claw_manager_state.device.id = g_claw_control_state.servo_id;
    g_claw_manager_state.device.online = feedback->online;
    g_claw_manager_state.device.error = feedback->error;
    g_claw_manager_state.device.last_online_tick = feedback->last_update_tick;

    g_claw_manager_state.feedback.position = feedback->position;
    g_claw_manager_state.feedback.angle_deg = feedback->angle_deg;
    g_claw_manager_state.feedback.speed = feedback->speed;
    g_claw_manager_state.feedback.load = feedback->load;
    g_claw_manager_state.feedback.load_abs = FeetechStsControl_AbsS16(feedback->load);
    if (g_claw_manager_state.feedback.update_count == 0UL) {
        g_claw_manager_state.feedback.angle_filtered_deg = feedback->angle_deg;
        g_claw_manager_state.feedback.load_abs_filtered = FeetechStsControl_AbsS16(feedback->load);
    } else {
        g_claw_manager_state.feedback.angle_filtered_deg = (g_claw_manager_state.feedback.angle_filtered_deg * 0.7f) + (feedback->angle_deg * 0.3f);
        g_claw_manager_state.feedback.load_abs_filtered = (uint16_t)(((uint32_t)g_claw_manager_state.feedback.load_abs_filtered * 7UL + (uint32_t)FeetechStsControl_AbsS16(feedback->load) * 3UL) / 10UL);
    }

    g_claw_manager_state.feedback.voltage_raw = feedback->voltage_raw;
    g_claw_manager_state.feedback.voltage_mv = feedback->voltage_mv;
    g_claw_manager_state.feedback.temperature_c = feedback->temperature_c;
    g_claw_manager_state.feedback.moving = feedback->moving;
    g_claw_manager_state.feedback.current_raw = feedback->current_raw;
    g_claw_manager_state.feedback.current_ma = feedback->current_ma;
    if (g_claw_manager_state.feedback.update_count == 0UL) {
        g_claw_manager_state.feedback.current_ma_filtered = feedback->current_ma;
    } else {
        g_claw_manager_state.feedback.current_ma_filtered = (uint16_t)(((uint32_t)g_claw_manager_state.feedback.current_ma_filtered * 7UL + (uint32_t)feedback->current_ma * 3UL) / 10UL);
    }

    g_claw_manager_state.feedback.position_valid = feedback->position_valid;
    g_claw_manager_state.feedback.telemetry_valid = feedback->telemetry_valid;
    g_claw_manager_state.feedback.update_flags = feedback->update_flags;
    g_claw_manager_state.feedback.update_count = feedback->rx_packet_count;
    g_claw_manager_state.feedback.update_tick = feedback->last_update_tick;

    g_claw_manager_state.target.target_angle_deg = g_claw_control_state.target_angle_deg;
    g_claw_manager_state.target.target_position = g_claw_control_state.target_position;
    g_claw_manager_state.target.target_time_ms = g_claw_control_state.target_time_ms;
    g_claw_manager_state.target.target_speed = g_claw_control_state.target_speed;
    g_claw_manager_state.target.target_acc = motion->target_acc;

    g_claw_manager_state.target.torque_limit = g_claw_control_state.target_torque_limit;
    g_claw_manager_state.target.torque_enable = FEETECH_STS_TORQUE_ENABLE;
    g_claw_manager_state.target.mode = FEETECH_STS_MODE_POSITION;
    g_claw_manager_state.target.reached = motion->reached;
    g_claw_manager_state.target.update_tick = bus->tx.tick_ms;

    g_claw_manager_state.action.state = g_claw_control_state.state;
    g_claw_manager_state.action.profile = g_claw_control_state.profile;
    g_claw_manager_state.action.result = g_claw_control_state.result;
    g_claw_manager_state.action.object_detected = g_claw_control_state.object_detected;

    g_claw_manager_state.action.sweep_active = g_claw_control_state.sweep_active;
    g_claw_manager_state.action.sweep_direction_close = g_claw_control_state.sweep_direction_close;
    g_claw_manager_state.action.last_motion_direction_close = g_claw_control_state.last_motion_direction_close;
    g_claw_manager_state.action.grip_trigger_counter = g_claw_control_state.grip_trigger_counter;
    g_claw_manager_state.action.overload_active = g_claw_control_state.overload_active;
    g_claw_manager_state.action.low_voltage_active = g_claw_control_state.low_voltage_active;
    g_claw_manager_state.action.release_permission = g_claw_control_state.release_permission;
    g_claw_manager_state.action.release_armed = g_claw_control_state.release_armed;
    g_claw_manager_state.action.slip_compensate_count = g_claw_control_state.slip_compensate_count;
    g_claw_manager_state.action.limit_setting_active = g_claw_control_state.limit_setting_active;
    g_claw_manager_state.action.limits_valid = g_claw_control_state.limits_valid;
    g_claw_manager_state.action.sweep_range = g_claw_control_state.sweep_range;

    g_claw_manager_state.action.state_tick_ms = g_claw_control_state.state_tick_ms;
    g_claw_manager_state.action.last_feedback_tick_ms = g_claw_control_state.last_feedback_tick_ms;

    g_claw_manager_state.comm.last_instruction = bus->tx.instruction;
    g_claw_manager_state.comm.last_address = bus->tx.address;
    g_claw_manager_state.comm.last_data_len = bus->tx.data_len;
    g_claw_manager_state.comm.last_error = feedback->error;
    g_claw_manager_state.comm.last_hal_status = bus->last_status;

    g_claw_manager_state.comm.tx_count = bus->tx.tx_count;
    g_claw_manager_state.comm.rx_count = feedback->rx_packet_count;
    g_claw_manager_state.comm.parse_error_count = feedback->parse_error_count;
    g_claw_manager_state.comm.last_tx_tick = bus->tx.tick_ms;
    g_claw_manager_state.comm.last_rx_tick = feedback->last_update_tick;

    g_claw_manager_state.authority.owner = g_claw_control_state.owner;
    g_claw_manager_state.authority.mode = g_claw_control_state.control_mode;
    g_claw_manager_state.authority.fault_level = g_claw_control_state.fault_level;
    g_claw_manager_state.authority.local_write_enable = g_claw_control_state.local_write_enable;
    g_claw_manager_state.authority.last_external_write_tick_ms = g_claw_control_state.last_external_write_tick_ms;
    g_claw_manager_state.authority.owner_timeout_ms = g_claw_control_state.owner_timeout_ms;
}

// 读取当前控制状态。
FeetechStsControlState_t FeetechStsControl_GetState(void)
{
    return g_claw_control_state.state;
}

// 读取目标角度。
float FeetechStsControl_GetTargetAngle(void)
{
    // 读取当前目标角度缓存。
    return g_claw_control_state.target_angle_deg;
}

// 读取实际角度。
float FeetechStsControl_GetActualAngle(void)
{
    // 实际角度只来自 STS 新鲜编码反馈。
    const FeetechStsFeedback_t *feedback;
    uint32_t now;

    now = HAL_GetTick();
    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U) &&
        ((uint32_t)(now - feedback->last_update_tick) <= FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS)) {
        return feedback->angle_deg;
    }


    return 0.0f;
}

// 读取反馈电压。
uint16_t FeetechStsControl_GetVoltageMv(void)
{
    // 读取反馈电压缓存。
    const FeetechStsFeedback_t *feedback;

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->telemetry_valid != 0U)) {
        return feedback->voltage_mv;
    }

    return 0U;
}

// 读取反馈温度。
uint8_t FeetechStsControl_GetTemperatureC(void)
{
    // 读取反馈温度缓存。
    const FeetechStsFeedback_t *feedback;

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->telemetry_valid != 0U)) {
        return feedback->temperature_c;
    }

    return 0U;
}

// 读取负载绝对值。
uint16_t FeetechStsControl_GetLoadAbs(void)
{
    // 读取当前负载绝对值缓存。
    return g_claw_control_state.present_load_abs;
}

// 读取当前电流。
uint16_t FeetechStsControl_GetCurrentMa(void)
{
    // 读取当前电流缓存。
    return g_claw_control_state.present_current_ma;
}

uint8_t FeetechStsControl_TakeSweepPrintEvent(FeetechStsSweepPrintEvent_t *out_event)
{
    if ((out_event == 0) || (s_sweep_print_event_ready == 0U)) {
        return 0U;
    }

    /* 扫描已经结束，丢弃旧的扫描打印事件。 */
    if ((g_claw_control_state.sweep_active == 0U) ||
        (g_claw_control_state.state != FEETECH_STS_CONTROL_SWEEP)) {
        s_sweep_print_event_ready = 0U;
        return 0U;
    }

    *out_event = s_sweep_print_event;
    s_sweep_print_event_ready = 0U;
    return 1U;
}

// 取走运动到位或卡死事件。
uint8_t FeetechStsControl_TakeMotionPrintEvent(FeetechStsMotionPrintEvent_t *out_event)
{
    if ((out_event == 0) || (s_motion_print_event_ready == 0U)) {
        return 0U;
    }
    *out_event = s_motion_print_event;
    s_motion_print_event_ready = 0U;
    return 1U;
}
// 读取状态文本。
const char *FeetechStsControl_GetStateText(void)
{
    switch (g_claw_control_state.state) {
        case FEETECH_STS_CONTROL_OPENING:
            return "OPEN";
        case FEETECH_STS_CONTROL_CLOSING:
            return "GRIP";
        case FEETECH_STS_CONTROL_GRIP_CONTACT:
            return "CONT";
        case FEETECH_STS_CONTROL_GRIP_SETTLE:
            return "SETL";
        case FEETECH_STS_CONTROL_GRIP_LOCKED:
            return "LOCK";
        case FEETECH_STS_CONTROL_CARRY_HOLD:
            return "CARRY";
        case FEETECH_STS_CONTROL_HOLDING:
            return "HOLD";
        case FEETECH_STS_CONTROL_RELEASE_ARMED:
            return "ARM";
        case FEETECH_STS_CONTROL_RELEASING:
            return "REL";
        case FEETECH_STS_CONTROL_SWEEP:
            return "SWP";
        case FEETECH_STS_CONTROL_GRIP_STEP_CONFIRM:
            return "STEP";
        case FEETECH_STS_CONTROL_NO_OBJECT:
            return "NONE";
        case FEETECH_STS_CONTROL_FAULT_HOLD:
            return "FHLT";
        case FEETECH_STS_CONTROL_ERROR:
            return "ERR";
        case FEETECH_STS_CONTROL_LIMIT_SETTING:
            return "CAL";
        case FEETECH_STS_CONTROL_IDLE:
        default:
            return "IDLE";
    }
}

// 读取控制权文本。
const char *FeetechStsControl_GetOwnerText(void)
{
    switch (g_claw_control_state.owner) {
        case FEETECH_STS_OWNER_KEY_A0:      return "KEYA";
        case FEETECH_STS_OWNER_KEY_B0:      return "KEYB";
        case FEETECH_STS_OWNER_VOFA:        return "VOFA";
        case FEETECH_STS_OWNER_BLUETOOTH:   return "BT";
        case FEETECH_STS_OWNER_USB_DIRECT:  return "USB";
        case FEETECH_STS_OWNER_SAFETY:      return "SAFE";
        case FEETECH_STS_OWNER_SCREEN:      return "UI";
        case FEETECH_STS_OWNER_NONE:
        default:                            return "NONE";
    }
}

// 读取控制模式文本。
const char *FeetechStsControl_GetModeText(void)
{
    switch (g_claw_control_state.control_mode) {
        case FEETECH_STS_CONTROL_MODE_MANUAL:       return "MAN";
        case FEETECH_STS_CONTROL_MODE_PASS_THROUGH: return "PASS";
        case FEETECH_STS_CONTROL_MODE_SAFE_HOLD:    return "SAFE";
        case FEETECH_STS_CONTROL_MODE_AUTO:
        default:                                    return "AUTO";
    }
}

// 读取故障原因文本。
const char *FeetechStsControl_GetFaultReasonText(void)
{
    if (g_claw_control_state.low_voltage_active != 0U) {
        return "LOWV";
    }

    switch (g_claw_control_state.result) {
        case FEETECH_STS_GRIP_RESULT_OVERLOAD:     return "OVLD";
        case FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE:  return "LOWV";
        case FEETECH_STS_GRIP_RESULT_COMM_ERROR:   return "COMM";
        case FEETECH_STS_GRIP_RESULT_TIMEOUT:      return "TIME";
        case FEETECH_STS_GRIP_RESULT_NO_OBJECT:    return "NONE";
        case FEETECH_STS_GRIP_RESULT_SLIP_PROTECT: return "SLIP";
        case FEETECH_STS_GRIP_RESULT_OBJECT:
        case FEETECH_STS_GRIP_RESULT_NONE:
        default:                                   return "----";
    }
}

// 读取当前控制权来源。
FeetechStsControlOwner_t FeetechStsControl_GetOwner(void)
{
    return g_claw_control_state.owner;
}

// 读取当前控制模式。
FeetechStsControlMode_t FeetechStsControl_GetMode(void)
{
    return g_claw_control_state.control_mode;
}

// 判断本地是否可写目标。
uint8_t FeetechStsControl_IsLocalWriteEnabled(void)
{
    // 读取本地目标写入许可。
    return g_claw_control_state.local_write_enable;
}

// 外部运动命令成为最新来源，并停止旧本地目标周期重发。
HAL_StatusTypeDef FeetechStsControl_ExternalWriteBegin(FeetechStsControlOwner_t owner)
{
    // 新控制命令覆盖旧来源；反馈和 manager 仍继续刷新。
    uint32_t now;

    now = HAL_GetTick();
    g_claw_control_state.owner = owner;
    g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_MANUAL;
    g_claw_control_state.local_write_enable = 0U;

    FeetechStsControl_ClearMotionRuntime(1U);

    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
    s_grip_detect_event_ready = 0U;
    s_sweep_print_event_ready = 0U;
    g_claw_control_state.last_external_write_tick_ms = now;
    g_claw_control_state.owner_timeout_ms = 0U;
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
    FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
    FeetechStsControl_SyncManager();
    return HAL_OK;
}

// 外部目标角度同步到控制层和 PWM 后端。
HAL_StatusTypeDef FeetechStsControl_CommandAngle(float angle_deg, FeetechStsControlOwner_t owner)
{
    HAL_StatusTypeDef status;
    float limited_angle;

    if (FeetechStsControl_IsMotionRangeReady() == 0U) {
        return HAL_ERROR;
    }

    limited_angle = FeetechStsControl_LimitFloatRange(angle_deg,
                                                       FeetechStsControl_GetMotionMinAngle(),
                                                       FeetechStsControl_GetMotionMaxAngle());
    (void)FeetechStsControl_ExternalWriteBegin(owner);

    g_claw_control_state.target_angle_deg = limited_angle;
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(limited_angle);
    g_claw_control_state.target_time_ms = 0U;
    g_claw_control_state.target_speed = g_claw_manager_state.config.default_speed;
    FeetechStsMotion_SetTargetEx(limited_angle,
                                 0U,
                                 g_claw_manager_state.config.default_speed,
                                 g_claw_manager_state.config.default_acc);

    status = FeetechStsControl_SendAngleToBackend(limited_angle,
                                                   0U,
                                                   g_claw_manager_state.config.default_speed,
                                                   g_claw_manager_state.config.default_acc);
    g_claw_control_state.last_status = status;
    if (status == HAL_OK) {
        FeetechStsControl_UpdateLastMotionDirection(FeetechStsControl_GetActualAngle(), limited_angle);
        FeetechStsControl_StartMotionTrack(HAL_GetTick());
    }
    if (status == HAL_OK) {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
    } else {
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
    }
    FeetechStsControl_SyncManager();
    return status;
}

// 更新自动动作的最新命令来源。
void FeetechStsControl_SetActionSource(FeetechStsControlOwner_t owner)
{
    g_claw_control_state.owner = owner;
    g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_AUTO;
    g_claw_control_state.local_write_enable = 1U;
    FeetechStsControl_SyncManager();
}

// 读取机械安全范围下限。
float FeetechStsControl_GetMotionMinAngle(void)
{
    float open_angle;
    float close_angle;

    if ((g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_FULL_270) ||
        (g_claw_control_state.limits_valid == 0U)) {
        return FEETECH_STS_CONTROL_B0_SWEEP_OPEN_ANGLE_DEG;
    }
    open_angle = g_claw_manager_state.config.open_angle_deg;
    close_angle = g_claw_manager_state.config.close_angle_deg;
    return (open_angle <= close_angle) ? open_angle : close_angle;
}

// 读取机械安全范围上限。
float FeetechStsControl_GetMotionMaxAngle(void)
{
    float open_angle;
    float close_angle;

    if ((g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_FULL_270) ||
        (g_claw_control_state.limits_valid == 0U)) {
        return FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG;
    }
    open_angle = g_claw_manager_state.config.open_angle_deg;
    close_angle = g_claw_manager_state.config.close_angle_deg;
    return (open_angle >= close_angle) ? open_angle : close_angle;
}

// 外部官方目标同步到控制层。
HAL_StatusTypeDef FeetechStsControl_ApplyExternalTarget(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc, FeetechStsControlOwner_t owner)
{
    float limited_angle;
    uint32_t now;

    now = HAL_GetTick();
    limited_angle = FeetechStsControl_LimitLocalAngle(angle_deg);

    g_claw_control_state.owner = owner;
    g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_MANUAL;
    g_claw_control_state.local_write_enable = 0U;
    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.release_armed = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
    s_grip_detect_event_ready = 0U;
    s_sweep_print_event_ready = 0U;
    g_claw_control_state.last_external_write_tick_ms = now;
    g_claw_control_state.owner_timeout_ms = 0U;

    g_claw_control_state.target_angle_deg = limited_angle;
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(limited_angle);
    g_claw_control_state.target_time_ms = time_ms;
    g_claw_control_state.target_speed = speed;

    FeetechStsMotion_SetTargetEx(limited_angle,
                                 time_ms,
                                 speed,
                                 acc);

    g_claw_control_state.last_status = HAL_OK;
    FeetechStsControl_UpdateLastMotionDirection(FeetechStsControl_GetActualAngle(), limited_angle);
    FeetechStsControl_StartMotionTrack(now);
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
    FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
    FeetechStsControl_SyncManager();
    return HAL_OK;
}


// 实际位置同步为目标缓存。
HAL_StatusTypeDef FeetechStsControl_SyncActualAsTarget(void)
{
    // 把有效反馈角度同步为本地目标缓存。
    const FeetechStsFeedback_t *feedback;
    float angle;

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U)) {
        angle = FeetechStsControl_LimitLocalAngle(feedback->angle_deg);
        g_claw_control_state.target_angle_deg = angle;
        g_claw_control_state.target_position = feedback->position;
        g_claw_control_state.hold_angle_deg = angle;
        g_claw_control_state.hold_position = feedback->position;
    } else {
        angle = g_claw_control_state.target_angle_deg;
    }

    g_claw_control_state.target_time_ms = g_claw_manager_state.config.default_move_time_ms;
    g_claw_control_state.target_speed = 0U;
    FeetechStsMotion_SetTargetEx(angle,
                                 g_claw_manager_state.config.default_move_time_ms,
                                 0U,
                                 g_claw_manager_state.config.default_acc);
    return HAL_OK;
}


/*============================================================
 * 6. 参数表接口
 *============================================================*/

// 读取可调参数数量。
uint8_t FeetechStsControl_GetParamCount(void)
{
    // 返回可调参数数量。
    return (uint8_t)FEETECH_STS_PARAM_COUNT;
}
// 读取参数名称。
const char *FeetechStsControl_GetParamName(FeetechStsParamId_t id)
{
    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:          return "OPEN_ANGLE";
        case FEETECH_STS_PARAM_CLOSE_ANGLE:         return "CLOSE_ANGLE";
        case FEETECH_STS_PARAM_PROFILE:             return "PROFILE";

        case FEETECH_STS_PARAM_DEFAULT_SPEED:       return "DEFAULT_SPEED";
        case FEETECH_STS_PARAM_OPEN_SPEED:          return "OPEN_SPEED";
        case FEETECH_STS_PARAM_CLOSE_SPEED:         return "CLOSE_SPEED";
        case FEETECH_STS_PARAM_SWEEP_SPEED:         return "SWEEP_SPEED";
        case FEETECH_STS_PARAM_SWEEP_STEP:          return "SWEEP_STEP";
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:       return "SWEEP_STEP_MS";

        case FEETECH_STS_PARAM_FULL_TORQUE:         return "FULL_TORQUE";
        case FEETECH_STS_PARAM_HOLD_TORQUE:         return "HOLD_TORQUE";

        case FEETECH_STS_PARAM_FEEDBACK_MS:         return "FEEDBACK_MS";
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS:    return "IDLE_FEEDBACK_MS";
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:      return "MIN_VALID_MOVE";
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:      return "GRIP_SETTLE_MS";

        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:      return "LOW_VOLTAGE_MV";
        case FEETECH_STS_PARAM_SLIP_ANGLE:          return "SLIP_ANGLE";
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:     return "SLIP_COMPENSATE";
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:  return "RELEASE_PERMISSION";

        case FEETECH_STS_PARAM_PRINT_ENABLE:        return "PRINT_ENABLE";
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:     return "PRINT_PERIOD_MS";
        default:                                    return "UNKNOWN";
    }
}
// 读取参数当前值。
uint8_t FeetechStsControl_GetParamValue(FeetechStsParamId_t id, int32_t *out_value)
{
    if (out_value == 0) {
        return 0U;
    }

    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:          *out_value = (int32_t)(g_claw_manager_state.config.open_angle_deg + 0.5f); break;
        case FEETECH_STS_PARAM_CLOSE_ANGLE:         *out_value = (int32_t)(g_claw_manager_state.config.close_angle_deg + 0.5f); break;
        case FEETECH_STS_PARAM_PROFILE:             *out_value = (int32_t)g_claw_control_state.profile; break;

        case FEETECH_STS_PARAM_DEFAULT_SPEED:       *out_value = (int32_t)g_claw_manager_state.config.default_speed; break;
        case FEETECH_STS_PARAM_OPEN_SPEED:          *out_value = (int32_t)g_claw_manager_state.config.open_speed; break;
        case FEETECH_STS_PARAM_CLOSE_SPEED:         *out_value = (int32_t)g_claw_manager_state.config.close_speed; break;
        case FEETECH_STS_PARAM_SWEEP_SPEED:         *out_value = (int32_t)g_claw_manager_state.config.sweep_speed; break;
        case FEETECH_STS_PARAM_SWEEP_STEP:          *out_value = (int32_t)(g_claw_manager_state.config.sweep_step_deg + 0.5f); break;
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:       *out_value = (int32_t)g_claw_manager_state.config.sweep_step_period_ms; break;

        case FEETECH_STS_PARAM_FULL_TORQUE:         *out_value = (int32_t)g_claw_manager_state.config.full_torque_limit; break;
        case FEETECH_STS_PARAM_HOLD_TORQUE:         *out_value = (int32_t)g_claw_manager_state.config.hold_torque_limit; break;

        case FEETECH_STS_PARAM_FEEDBACK_MS:         *out_value = (int32_t)g_claw_manager_state.config.grip_feedback_period_ms; break;
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS:    *out_value = (int32_t)g_claw_manager_state.config.idle_feedback_period_ms; break;
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:      *out_value = (int32_t)(g_claw_manager_state.config.grip_min_valid_move_deg + 0.5f); break;
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:      *out_value = (int32_t)g_claw_manager_state.config.grip_settle_ms; break;

        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:      *out_value = (int32_t)g_claw_manager_state.config.low_voltage_mv; break;
        case FEETECH_STS_PARAM_SLIP_ANGLE:          *out_value = (int32_t)(g_claw_manager_state.config.slip_angle_deg + 0.5f); break;
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:     *out_value = (int32_t)(g_claw_manager_state.config.slip_compensate_deg + 0.5f); break;
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:  *out_value = (int32_t)g_claw_control_state.release_permission; break;

        case FEETECH_STS_PARAM_PRINT_ENABLE:        *out_value = (int32_t)g_claw_manager_state.option.print_enable; break;
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:     *out_value = (int32_t)g_claw_manager_state.option.print_period_ms; break;
        default: return 0U;
    }

    return 1U;
}
// 读取可调参数信息。
uint8_t FeetechStsControl_GetParamInfo(FeetechStsParamId_t id, FeetechStsParamInfo_t *out_info)
{
    // 按参数 ID 导出名称、当前值和范围。
    int32_t value;

    if ((out_info == 0) || (id >= FEETECH_STS_PARAM_COUNT)) {
        return 0U;
    }

    if (FeetechStsControl_GetParamValue(id, &value) == 0U) {
        return 0U;
    }

    out_info->id = id;
    out_info->name = FeetechStsControl_GetParamName(id);
    out_info->value = value;
    out_info->default_value = FeetechStsControl_ParamLimit(id, INT32_MIN);
    out_info->min_value = FeetechStsControl_ParamLimit(id, INT32_MIN + 1);
    out_info->max_value = FeetechStsControl_ParamLimit(id, INT32_MAX);
    out_info->step = 1;
    out_info->writable = 1U;
    return 1U;
}
// 写入可调参数值。
uint8_t FeetechStsControl_SetParamValue(FeetechStsParamId_t id, int32_t value)
{
    // 写入参数值，并按范围裁剪后同步运行配置。
    if (id >= FEETECH_STS_PARAM_COUNT) {
        return 0U;
    }

    value = FeetechStsControl_ParamLimit(id, value);

    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:          g_claw_manager_state.config.open_angle_deg = (float)value; break;
        case FEETECH_STS_PARAM_CLOSE_ANGLE:         g_claw_manager_state.config.close_angle_deg = (float)value; break;
        case FEETECH_STS_PARAM_PROFILE:             (void)FeetechStsControl_SetProfile((FeetechStsGripProfile_t)value); break;

        case FEETECH_STS_PARAM_DEFAULT_SPEED:       g_claw_manager_state.config.default_speed = (uint16_t)value; break;
        case FEETECH_STS_PARAM_OPEN_SPEED:          g_claw_manager_state.config.open_speed = (uint16_t)value; break;
        case FEETECH_STS_PARAM_CLOSE_SPEED:         g_claw_manager_state.config.close_speed = (uint16_t)value; break;
        case FEETECH_STS_PARAM_SWEEP_SPEED:         g_claw_manager_state.config.sweep_speed = (uint16_t)value; break;
        case FEETECH_STS_PARAM_SWEEP_STEP:          g_claw_manager_state.config.sweep_step_deg = (float)value; break;
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:       g_claw_manager_state.config.sweep_step_period_ms = (uint16_t)value; break;

        case FEETECH_STS_PARAM_FULL_TORQUE:         g_claw_manager_state.config.full_torque_limit = (uint16_t)value; break;
        case FEETECH_STS_PARAM_HOLD_TORQUE:         g_claw_manager_state.config.hold_torque_limit = (uint16_t)value; break;

        case FEETECH_STS_PARAM_FEEDBACK_MS:         g_claw_manager_state.config.feedback_period_ms = (uint16_t)value; g_claw_manager_state.config.grip_feedback_period_ms = (uint16_t)value; break;
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS:    g_claw_manager_state.config.idle_feedback_period_ms = (uint16_t)value; break;
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:      g_claw_manager_state.config.grip_min_valid_move_deg = (float)value; break;
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:      g_claw_manager_state.config.grip_settle_ms = (uint16_t)value; break;

        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:      g_claw_manager_state.config.low_voltage_mv = (uint16_t)value; break;
        case FEETECH_STS_PARAM_SLIP_ANGLE:          g_claw_manager_state.config.slip_angle_deg = (float)value; break;
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:     g_claw_manager_state.config.slip_compensate_deg = (float)value; break;
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:  g_claw_control_state.release_permission = (value == 0) ? 0U : 1U; break;

        case FEETECH_STS_PARAM_PRINT_ENABLE:        g_claw_manager_state.option.print_enable = (value == 0) ? 0U : 1U; break;
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:     g_claw_manager_state.option.print_period_ms = (uint16_t)value; break;
        default: return 0U;
    }

    FeetechStsControl_ApplyConfigToRuntime();
    return 1U;
}
// 复位单个参数。
uint8_t FeetechStsControl_ResetParamValue(FeetechStsParamId_t id)
{
    // 恢复单个参数默认值并重新应用配置。
    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:       return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_OPEN_ANGLE_DEG);
        case FEETECH_STS_PARAM_CLOSE_ANGLE:      return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_CLOSE_ANGLE_DEG);
        case FEETECH_STS_PARAM_PROFILE:          return FeetechStsControl_SetParamValue(id, FEETECH_STS_GRIP_PROFILE_MEDIUM);
        case FEETECH_STS_PARAM_DEFAULT_SPEED:    return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_DEFAULT_SPEED);
        case FEETECH_STS_PARAM_OPEN_SPEED:       return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_A0_OPEN_SPEED);
        case FEETECH_STS_PARAM_CLOSE_SPEED:      return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_A0_CLOSE_SPEED);
        case FEETECH_STS_PARAM_SWEEP_SPEED:      return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_B0_SWEEP_SPEED);
        case FEETECH_STS_PARAM_SWEEP_STEP:       return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_SWEEP_STEP_DEG);
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:    return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_SWEEP_STEP_PERIOD_MS);
        case FEETECH_STS_PARAM_FULL_TORQUE:      return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_GRIP_FULL_TORQUE);
        case FEETECH_STS_PARAM_HOLD_TORQUE:      return FeetechStsControl_SetParamValue(id, s_grip_profiles[FEETECH_STS_GRIP_PROFILE_MEDIUM].hold_torque_limit);
        case FEETECH_STS_PARAM_FEEDBACK_MS:      return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_FEEDBACK_PERIOD_MS);
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS: return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_IDLE_FEEDBACK_MS);
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:   return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_GRIP_MIN_VALID_MOVE_DEG);
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:   return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_GRIP_SETTLE_MS);
        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:   return FeetechStsControl_SetParamValue(id, FEETECH_STS_CONTROL_LOW_VOLTAGE_MV);
        case FEETECH_STS_PARAM_SLIP_ANGLE:       return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_SLIP_ANGLE_DEG);
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:  return FeetechStsControl_SetParamValue(id, (int32_t)FEETECH_STS_CONTROL_SLIP_COMPENSATE_DEG);
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:return FeetechStsControl_SetParamValue(id, 1);
        case FEETECH_STS_PARAM_PRINT_ENABLE:     return FeetechStsControl_SetParamValue(id, FEETECH_STS_MANAGER_PRINT_DEFAULT);
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:  return FeetechStsControl_SetParamValue(id, FEETECH_STS_MANAGER_PRINT_PERIOD_MS);
        default: return 0U;
    }
}

// 复位全部可调参数。
void FeetechStsControl_ResetAllParams(void)
{
    // 恢复全部参数默认值并重新应用配置。
    FeetechStsControl_LoadDefaultParams();
    FeetechStsControl_ApplyConfigToRuntime();
}

// 参数值限幅。
static int32_t FeetechStsControl_ParamLimit(FeetechStsParamId_t id, int32_t value)
{
    // 按参数 ID 对输入值做范围限制。
    int32_t min_value;
    int32_t max_value;
    int32_t default_value;

    min_value = 0;
    max_value = 10000;
    default_value = 0;

    switch (id) {
        case FEETECH_STS_PARAM_OPEN_ANGLE:       max_value = (int32_t)FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG; default_value = (int32_t)FEETECH_STS_CONTROL_OPEN_ANGLE_DEG; break;
        case FEETECH_STS_PARAM_CLOSE_ANGLE:      max_value = (int32_t)FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG; default_value = (int32_t)FEETECH_STS_CONTROL_CLOSE_ANGLE_DEG; break;
        case FEETECH_STS_PARAM_PROFILE:          max_value = (int32_t)(FEETECH_STS_GRIP_PROFILE_COUNT - 1U); default_value = FEETECH_STS_GRIP_PROFILE_MEDIUM; break;
        case FEETECH_STS_PARAM_DEFAULT_SPEED:    max_value = FEETECH_STS_SPEED_MAX; default_value = FEETECH_STS_CONTROL_DEFAULT_SPEED; break;
        case FEETECH_STS_PARAM_OPEN_SPEED:       max_value = FEETECH_STS_SPEED_MAX; default_value = FEETECH_STS_CONTROL_A0_OPEN_SPEED; break;
        case FEETECH_STS_PARAM_CLOSE_SPEED:      max_value = FEETECH_STS_SPEED_MAX; default_value = FEETECH_STS_CONTROL_A0_CLOSE_SPEED; break;
        case FEETECH_STS_PARAM_SWEEP_SPEED:      max_value = FEETECH_STS_SPEED_MAX; default_value = FEETECH_STS_CONTROL_B0_SWEEP_SPEED; break;
        case FEETECH_STS_PARAM_SWEEP_STEP:       min_value = 1; max_value = 90; default_value = (int32_t)FEETECH_STS_CONTROL_SWEEP_STEP_DEG; break;
        case FEETECH_STS_PARAM_SWEEP_STEP_MS:    min_value = 20; max_value = 1000; default_value = FEETECH_STS_CONTROL_SWEEP_STEP_PERIOD_MS; break;
        case FEETECH_STS_PARAM_FULL_TORQUE:      max_value = FEETECH_STS_TORQUE_LIMIT_MAX; default_value = FEETECH_STS_CONTROL_GRIP_FULL_TORQUE; break;
        case FEETECH_STS_PARAM_HOLD_TORQUE:      max_value = FEETECH_STS_TORQUE_LIMIT_MAX; default_value = s_grip_profiles[FEETECH_STS_GRIP_PROFILE_MEDIUM].hold_torque_limit; break;
        case FEETECH_STS_PARAM_FEEDBACK_MS:      min_value = 5; max_value = 1000; default_value = FEETECH_STS_CONTROL_FEEDBACK_PERIOD_MS; break;
        case FEETECH_STS_PARAM_IDLE_FEEDBACK_MS: min_value = 20; max_value = 5000; default_value = FEETECH_STS_CONTROL_IDLE_FEEDBACK_MS; break;
        case FEETECH_STS_PARAM_MIN_VALID_MOVE:   min_value = 0; max_value = 45; default_value = (int32_t)FEETECH_STS_CONTROL_GRIP_MIN_VALID_MOVE_DEG; break;
        case FEETECH_STS_PARAM_GRIP_SETTLE_MS:   min_value = 0; max_value = 3000; default_value = FEETECH_STS_CONTROL_GRIP_SETTLE_MS; break;
        case FEETECH_STS_PARAM_LOW_VOLTAGE_MV:   min_value = 0; max_value = 30000; default_value = FEETECH_STS_CONTROL_LOW_VOLTAGE_MV; break;
        case FEETECH_STS_PARAM_SLIP_ANGLE:       min_value = 1; max_value = 45; default_value = (int32_t)FEETECH_STS_CONTROL_SLIP_ANGLE_DEG; break;
        case FEETECH_STS_PARAM_SLIP_COMPENSATE:  min_value = 1; max_value = 30; default_value = (int32_t)FEETECH_STS_CONTROL_SLIP_COMPENSATE_DEG; break;
        case FEETECH_STS_PARAM_RELEASE_PERMISSION:max_value = 1; default_value = 1; break;
        case FEETECH_STS_PARAM_PRINT_ENABLE:     max_value = 1; default_value = FEETECH_STS_MANAGER_PRINT_DEFAULT; break;
        case FEETECH_STS_PARAM_PRINT_PERIOD_MS:  min_value = 100; max_value = 10000; default_value = FEETECH_STS_MANAGER_PRINT_PERIOD_MS; break;
        default: break;
    }

    if (value == INT32_MIN) {
        return default_value;
    }

    if (value == (INT32_MIN + 1)) {
        return min_value;
    }

    if (value == INT32_MAX) {
        return max_value;
    }

    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

// 启动新目标的编码器运动跟踪。
static void FeetechStsControl_StartMotionTrack(uint32_t now)
{
    const FeetechStsFeedback_t *feedback;

    feedback = FeetechStsBus_GetFeedback();
    g_claw_control_state.motion_track_active = 1U;
    g_claw_control_state.motion_start_tick_ms = now;
    g_claw_control_state.stall_reference_tick_ms = now;
    g_claw_control_state.stall_last_feedback_tick_ms = 0UL;
    g_claw_control_state.stall_reference_position =
        ((feedback != 0) && (feedback->position_valid != 0U)) ? feedback->position : g_claw_control_state.target_position;
    s_motion_print_event_ready = 0U;
}

// 记录正常到位或卡死事件。
static void FeetechStsControl_RecordMotionPrintEvent(uint8_t stalled, const FeetechStsFeedback_t *feedback)
{
    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        return;
    }
    s_motion_print_event.stalled = (stalled != 0U) ? 1U : 0U;
    s_motion_print_event.angle_deg = feedback->angle_deg;
    s_motion_print_event.position = feedback->position;
    s_motion_print_event.target_position = g_claw_control_state.target_position;
    s_motion_print_event_ready = 1U;
}

// 统一完成运动：记录到位事件、停止跟踪并进入结束状态。
static void FeetechStsControl_FinishMotionReached(
    const FeetechStsFeedback_t *feedback,
    FeetechStsControlState_t finish_state)
{
    /*
     * 只有本次运动仍在跟踪时记录，
     * 防止TaskStall与TaskRelease重复打印两次到位。
     */
    if ((g_claw_control_state.motion_track_active != 0U) &&
        (feedback != 0) &&
        (feedback->position_valid != 0U)) {

        FeetechStsControl_RecordMotionPrintEvent(
            0U,
            feedback);
    }

    /*
     * 使用0U，不清除刚刚记录的正常到位事件。
     * ClearMotionRuntime(1U)会把事件一起删除。
     */
    FeetechStsControl_ClearMotionRuntime(0U);

    FeetechStsControl_SetResult(
        FEETECH_STS_GRIP_RESULT_NONE);

    FeetechStsControl_SetState(finish_state);
}

// 直接以最新反馈位置停止，不改变命令来源。


// 计算停止保持编码：在当前运动方向上进行少量前向补偿。
static uint16_t FeetechStsControl_GetStopHoldPosition(
    const FeetechStsFeedback_t *feedback)
{
    uint16_t stop_position;
    uint16_t open_position;
    uint16_t close_position;
    uint16_t min_position;
    uint16_t max_position;
    uint16_t lead_position;

    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        return g_claw_control_state.target_position;
    }

    stop_position = feedback->position;
    lead_position = FEETECH_STS_CONTROL_STOP_LEAD_POSITION;

    open_position =
        FeetechSts_AngleToPosition(FeetechStsControl_GetMotionOpenAngle());
    close_position =
        FeetechSts_AngleToPosition(FeetechStsControl_GetMotionCloseAngle());

    min_position = (open_position <= close_position) ?
                   open_position : close_position;
    max_position = (open_position >= close_position) ?
                   open_position : close_position;

    /*
     * 当前目标比反馈位置大，说明正在向编码增大方向运动；
     * 当前目标比反馈位置小，说明正在向编码减小方向运动。
     */
    if (g_claw_control_state.target_position > feedback->position) {
        if ((uint32_t)stop_position + lead_position >
            (uint32_t)max_position) {
            stop_position = max_position;
        } else {
            stop_position =
                (uint16_t)(stop_position + lead_position);
        }
    } else if (g_claw_control_state.target_position <
               feedback->position) {
        if (stop_position <
            (uint16_t)(min_position + lead_position)) {
            stop_position = min_position;
        } else {
            stop_position =
                (uint16_t)(stop_position - lead_position);
        }
    }

    return stop_position;
}

// 停止动作：使用反馈位置加前向补偿，避免写回旧位置后回退。
static HAL_StatusTypeDef FeetechStsControl_HoldStopPosition(
    const FeetechStsFeedback_t *feedback)
{
    uint16_t stop_position;
    float stop_angle;

    if ((feedback == 0) ||
        (feedback->position_valid == 0U)) {
        return HAL_ERROR;
    }

    stop_position =
        FeetechStsControl_GetStopHoldPosition(feedback);
    stop_angle =
        FeetechSts_PositionToAngle(stop_position);

    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.motion_track_active = 0U;

    g_claw_control_state.target_position = stop_position;
    g_claw_control_state.target_angle_deg = stop_angle;
    g_claw_control_state.hold_position = stop_position;
    g_claw_control_state.hold_angle_deg = stop_angle;

    g_claw_control_state.target_time_ms = 0U;
    g_claw_control_state.target_speed = 0U;

    FeetechStsMotion_SetTargetEx(
        stop_angle,
        0U,
        0U,
        g_claw_manager_state.config.default_acc);

    g_claw_control_state.last_status =
        FeetechStsControl_SendPositionToBackend(
            stop_position,
            stop_angle,
            0U,
            0U,
            g_claw_manager_state.config.default_acc);

    FeetechStsControl_SetState(
        FEETECH_STS_CONTROL_HOLDING);

    return g_claw_control_state.last_status;
}

// 保存最近有效运动方向。
static void FeetechStsControl_UpdateLastMotionDirection(float current_angle, float target_angle)
{
    float open_angle;
    float close_angle;

    open_angle = FeetechStsControl_GetMotionOpenAngle();
    close_angle = FeetechStsControl_GetMotionCloseAngle();
    if (close_angle >= open_angle) {
        g_claw_control_state.last_motion_direction_close = (target_angle >= current_angle) ? 1U : 0U;
    } else {
        g_claw_control_state.last_motion_direction_close = (target_angle <= current_angle) ? 1U : 0U;
    }
}

// 通用到位和卡死判断。
static void FeetechStsControl_TaskStall(uint32_t now)
{
    const FeetechStsFeedback_t *feedback;
    uint16_t delta;
    uint16_t target_delta;

    if ((FEETECH_STS_CONTROL_STALL_ENABLE == 0U) ||
        (g_claw_control_state.motion_track_active == 0U) ||
        (g_claw_control_state.limit_setting_active != 0U)) {
        return;
    }
    feedback = FeetechStsBus_GetFeedback();
    if ((feedback == 0) || (feedback->position_valid == 0U) ||
        (feedback->last_update_tick == g_claw_control_state.stall_last_feedback_tick_ms)) {
        return;
    }
    g_claw_control_state.stall_last_feedback_tick_ms = feedback->last_update_tick;

    target_delta = (feedback->position >= g_claw_control_state.target_position) ?
                   (uint16_t)(feedback->position - g_claw_control_state.target_position) :
                   (uint16_t)(g_claw_control_state.target_position - feedback->position);
    if (target_delta <= g_claw_manager_state.config.stall_target_margin_position) {
        g_claw_control_state.motion_track_active = 0U;
        if (g_claw_control_state.state != FEETECH_STS_CONTROL_SWEEP) {
            FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
            FeetechStsControl_RecordMotionPrintEvent(0U, feedback);
        }
        return;
    }

    if ((uint32_t)(now - g_claw_control_state.motion_start_tick_ms) <
        (uint32_t)g_claw_manager_state.config.stall_startup_ms) {
        g_claw_control_state.stall_reference_position = feedback->position;
        g_claw_control_state.stall_reference_tick_ms = now;
        return;
    }

    delta = (feedback->position >= g_claw_control_state.stall_reference_position) ?
            (uint16_t)(feedback->position - g_claw_control_state.stall_reference_position) :
            (uint16_t)(g_claw_control_state.stall_reference_position - feedback->position);
    if (delta >= g_claw_manager_state.config.stall_min_delta_position) {
        g_claw_control_state.stall_reference_position = feedback->position;
        g_claw_control_state.stall_reference_tick_ms = now;
        return;
    }
    if ((uint32_t)(now - g_claw_control_state.stall_reference_tick_ms) <
        (uint32_t)g_claw_manager_state.config.stall_timeout_ms) {
        return;
    }

    FeetechStsControl_RecordMotionPrintEvent(1U, feedback);
    (void)FeetechStsControl_HoldStopPosition(feedback);
}
/*============================================================
 * 6. 内部周期任务
 *============================================================*/

// 周期读取并同步舵机反馈。
static void FeetechStsControl_TaskFeedback(uint32_t now)
{
    // 反馈入口：按模式选周期 -> READ 位置/电参 -> 缓存真实编码值。
    uint32_t period;
    HAL_StatusTypeDef status;


    if (s_feedback_retry_active != 0U) {
        period = FEETECH_STS_CONTROL_FEEDBACK_RETRY_MS;
    } else if ((g_claw_control_state.control_mode == FEETECH_STS_CONTROL_MODE_MANUAL) ||
               (g_claw_control_state.control_mode == FEETECH_STS_CONTROL_MODE_PASS_THROUGH)) {
        period = FEETECH_STS_CONTROL_PASS_FEEDBACK_MS;
    } else {
        period = ((g_claw_control_state.state == FEETECH_STS_CONTROL_CLOSING) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_CONTACT) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_SETTLE) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_LOCKED) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_CARRY_HOLD) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_FAULT_HOLD) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_HOLDING) ||
                  (g_claw_control_state.state == FEETECH_STS_CONTROL_SWEEP)) ?
                 g_claw_manager_state.config.grip_feedback_period_ms : g_claw_manager_state.config.idle_feedback_period_ms;
    }

    if ((uint32_t)(now - g_claw_control_state.last_feedback_tick_ms) >= period) {
        g_claw_control_state.last_feedback_tick_ms = now;
        status = FeetechStsControl_RequestFeedback();
        s_feedback_retry_active = (status == HAL_OK) ? 0U : 1U;
    }
}
// 释放动作完成检测。
static void FeetechStsControl_TaskRelease(uint32_t now)
{
    // 释放态：到达张开角或超时后回到空闲。
    const FeetechStsFeedback_t *feedback;
    (void)now;

    if (g_claw_control_state.state != FEETECH_STS_CONTROL_RELEASING) {
        return;
    }


    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U)) {
        if ((feedback->moving == 0U) && (FeetechStsControl_IsNearCloseLimit(feedback, FeetechStsControl_GetProfileConfig(g_claw_control_state.profile)) == 0U)) {
            FeetechStsControl_FinishMotionReached(feedback,FEETECH_STS_CONTROL_IDLE);
        }
    }
}

// B0 扫描状态机。
static void FeetechStsControl_TaskSweep(uint32_t now)
{
    // STS 扫描仅由真实到位反馈换向。
    const FeetechStsFeedback_t *feedback;
    float diff;
    uint32_t elapsed;
    uint8_t reached;

    if ((g_claw_control_state.sweep_active == 0U) || (g_claw_control_state.state != FEETECH_STS_CONTROL_SWEEP)) {
        return;
    }

    elapsed = (uint32_t)(now - g_claw_control_state.state_tick_ms);
    if (elapsed < (uint32_t)FEETECH_STS_CONTROL_SWEEP_HOLD_MS) {
        return;
    }

    reached = 0U;
    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U) &&
        ((uint32_t)(now - feedback->last_update_tick) <= FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS)) {
        diff = FeetechStsControl_AbsFloatDiff(feedback->angle_deg, g_claw_control_state.target_angle_deg);
        if ((diff <= 3.0f) && (feedback->moving == 0U)) {
            reached = 1U;
        }
    }

    if (reached == 0U) {
        return;
    }

    (void)FeetechStsControl_SweepStep(now, "B0换向");
}
// 扫描方向切换并下发目标。
static HAL_StatusTypeDef FeetechStsControl_SweepStep(uint32_t now, const char *event_text)
{
    // 真实当前位置 -> 计算下一端点 -> 发送一次目标。
    const FeetechStsFeedback_t *feedback;
    float current_angle;
    float next_angle;
    uint16_t sweep_speed;

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback != 0) && (feedback->position_valid != 0U) &&
        ((uint32_t)(now - feedback->last_update_tick) <= FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS)) {
        current_angle = feedback->angle_deg;
    } else {
        g_claw_control_state.last_status = HAL_TIMEOUT;
        return HAL_TIMEOUT;
    }

    if (g_claw_control_state.sweep_direction_close != 0U) {
        next_angle = FeetechStsControl_GetMotionCloseAngle();
        g_claw_control_state.last_motion_direction_close = 1U;
        g_claw_control_state.sweep_direction_close = 0U;
    } else {
        next_angle = FeetechStsControl_GetMotionOpenAngle();
        g_claw_control_state.last_motion_direction_close = 0U;
        g_claw_control_state.sweep_direction_close = 1U;
    }

    sweep_speed = (g_claw_control_state.limit_setting_active != 0U) ?
                  g_claw_manager_state.config.calibration_sweep_speed :
                  g_claw_manager_state.config.sweep_speed;
    g_claw_control_state.target_time_ms = 0U;
    g_claw_control_state.target_speed = sweep_speed;

    g_claw_control_state.last_status = FeetechStsControl_MoveToAngleWithSpeed(next_angle,
                                                                              0U,
                                                                              sweep_speed,
                                                                              FEETECH_STS_CONTROL_B0_SWEEP_ACC);
    if (g_claw_control_state.last_status == HAL_OK) {
        feedback = FeetechStsBus_GetFeedback();
        if (feedback != 0) {
            g_claw_control_state.grip_last_rx_packet_count = feedback->rx_packet_count;
        } else {
            g_claw_control_state.grip_last_rx_packet_count = 0UL;
        }
        FeetechStsControl_ResetGripStableState(feedback);
        FeetechStsControl_RecordSweepPrintEvent(event_text, current_angle, next_angle);
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_SWEEP);
    }

    return g_claw_control_state.last_status;
}
/*============================================================
 * 7. 统一信息管理结构体初始化
 *============================================================*/

// 清空并初始化控制管理器。
static void FeetechStsControl_InitManager(void)
{
    // 初始化管理结构体指针、配置和运行快照。
    memset(&g_claw_manager_state, 0, sizeof(g_claw_manager_state));

    FeetechStsControl_LoadDefaultParams();

    g_claw_manager_state.option.last_print_tick = 0UL;
    g_claw_manager_state.option.screen_update_request = 1U;
}

// 加载默认可调参数。
static void FeetechStsControl_LoadDefaultParams(void)
{
    // 加载默认动作、力矩、反馈周期等参数。
    g_claw_manager_state.config.servo_id = FEETECH_STS_CONTROL_SERVO_ID;

    g_claw_manager_state.config.open_angle_deg = FEETECH_STS_CONTROL_OPEN_ANGLE_DEG;
    g_claw_manager_state.config.close_angle_deg = FEETECH_STS_CONTROL_CLOSE_ANGLE_DEG;

    g_claw_manager_state.config.default_move_time_ms = FEETECH_STS_CONTROL_MOVE_TIME_MS;
    g_claw_manager_state.config.default_release_time_ms = FEETECH_STS_CONTROL_RELEASE_TIME_MS;
    g_claw_manager_state.config.default_sweep_time_ms = FEETECH_STS_CONTROL_SWEEP_TIME_MS;
    g_claw_manager_state.config.default_grip_time_ms = FEETECH_STS_CONTROL_GRIP_TIME_MS;

    g_claw_manager_state.config.default_speed = FEETECH_STS_CONTROL_DEFAULT_SPEED;
    g_claw_manager_state.config.open_speed = FEETECH_STS_CONTROL_A0_OPEN_SPEED;
    g_claw_manager_state.config.close_speed = FEETECH_STS_CONTROL_A0_CLOSE_SPEED;
    g_claw_manager_state.config.sweep_speed = FEETECH_STS_CONTROL_B0_SWEEP_SPEED;
    g_claw_manager_state.config.calibration_sweep_speed =
        g_claw_feature_defaults.calibration_sweep_speed;
    g_claw_manager_state.config.default_acc = FEETECH_STS_CONTROL_DEFAULT_ACC;

    g_claw_manager_state.config.full_torque_limit = FEETECH_STS_CONTROL_GRIP_FULL_TORQUE;
    g_claw_manager_state.config.approach_torque_limit = s_grip_profiles[FEETECH_STS_GRIP_PROFILE_MEDIUM].grip_torque_limit;
    g_claw_manager_state.config.contact_torque_limit = s_grip_profiles[FEETECH_STS_GRIP_PROFILE_MEDIUM].grip_torque_limit;
    g_claw_manager_state.config.hold_torque_limit = s_grip_profiles[FEETECH_STS_GRIP_PROFILE_MEDIUM].hold_torque_limit;

    g_claw_manager_state.config.feedback_period_ms = FEETECH_STS_CONTROL_FEEDBACK_PERIOD_MS;
    g_claw_manager_state.config.idle_feedback_period_ms = FEETECH_STS_CONTROL_IDLE_FEEDBACK_MS;
    g_claw_manager_state.config.grip_feedback_period_ms = FEETECH_STS_CONTROL_FEEDBACK_PERIOD_MS;

    g_claw_manager_state.config.grip_min_valid_move_deg = FEETECH_STS_CONTROL_GRIP_MIN_VALID_MOVE_DEG;
    g_claw_manager_state.config.grip_contact_ms = FEETECH_STS_CONTROL_GRIP_CONTACT_MS;
    g_claw_manager_state.config.grip_settle_ms = FEETECH_STS_CONTROL_GRIP_SETTLE_MS;

    g_claw_manager_state.config.sweep_step_deg = FEETECH_STS_CONTROL_SWEEP_STEP_DEG;
    g_claw_manager_state.config.sweep_step_period_ms = FEETECH_STS_CONTROL_SWEEP_STEP_PERIOD_MS;

    g_claw_manager_state.config.low_voltage_mv = FEETECH_STS_CONTROL_LOW_VOLTAGE_MV;
    g_claw_manager_state.config.slip_angle_deg = FEETECH_STS_CONTROL_SLIP_ANGLE_DEG;
    g_claw_manager_state.config.slip_load_drop = FEETECH_STS_CONTROL_SLIP_LOAD_DROP;
    g_claw_manager_state.config.slip_compensate_deg = FEETECH_STS_CONTROL_SLIP_COMPENSATE_DEG;
    g_claw_manager_state.config.slip_compensate_period_ms = FEETECH_STS_CONTROL_SLIP_COMPENSATE_PERIOD_MS;

    g_claw_control_state.release_permission = 1U;

    g_claw_manager_state.option.print_enable = g_claw_feature_defaults.auto_print_enable;
    g_claw_manager_state.option.print_period_ms = g_claw_feature_defaults.auto_print_period_ms;
}

// 配置参数 -> 运行时缓存。
static void FeetechStsControl_ApplyConfigToRuntime(void)
{
    // 把 STS 配置项应用到运行态目标和屏幕刷新请求。
    g_claw_control_state.servo_id = g_claw_manager_state.config.servo_id;
    g_claw_control_state.target_angle_deg = FeetechStsControl_LimitLocalAngle(g_claw_control_state.target_angle_deg);
    g_claw_control_state.target_position = FeetechSts_AngleToPosition(g_claw_control_state.target_angle_deg);

    g_claw_manager_state.option.screen_update_request = 1U;
}

// 按键新动作成为最新来源，并恢复本地状态机下发。
static HAL_StatusTypeDef FeetechStsControl_RequestLocalOwner(FeetechStsControlOwner_t owner)
{
    // 新本机控制命令覆盖旧来源。
    if ((g_claw_control_state.state == FEETECH_STS_CONTROL_FAULT_HOLD) &&
        (FeetechStsControl_CanClearFaultByKey() == 0U) &&
        (owner != FEETECH_STS_OWNER_SAFETY)) {
        return HAL_ERROR;
    }

    if (g_claw_control_state.state == FEETECH_STS_CONTROL_FAULT_HOLD) {
        FeetechStsControl_ClearSoftFault();
    }

    g_claw_control_state.owner = owner;
    g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_AUTO;
    g_claw_control_state.local_write_enable = 1U;
    g_claw_control_state.owner_timeout_ms = FEETECH_STS_CONTROL_EXTERNAL_TIMEOUT_MS;
    return HAL_OK;
}

// 判断最新命令是否允许本地状态机继续下发目标。
static uint8_t FeetechStsControl_CanLocalWriteTarget(void)
{
    // 本地按键是最新运动来源时，状态机才继续下发目标。
    return g_claw_control_state.local_write_enable;
}

// 判断当前是否仍需周期维护运动目标。
static uint8_t FeetechStsControl_ShouldRunMotionTask(void)
{
    if (FeetechStsControl_CanLocalWriteTarget() == 0U) {
        return 0U;
    }

    if ((g_claw_control_state.motion_track_active != 0U) ||
        (g_claw_control_state.sweep_active != 0U)) {
        return 1U;
    }

    return 0U;
}

// 判断按键是否可清软故障。
static uint8_t FeetechStsControl_CanClearFaultByKey(void)
{
    // 判断按键是否允许清除软故障。
    if (g_claw_control_state.fault_level == FEETECH_STS_FAULT_LEVEL_HARD) {
        return 0U;
    }

    if (g_claw_control_state.low_voltage_active != 0U) {
        return 0U;
    }

    if (g_claw_control_state.result == FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE) {
        return 0U;
    }

    if (g_claw_control_state.result == FEETECH_STS_GRIP_RESULT_COMM_ERROR) {
        return 0U;
    }

    return 1U;
}

// 清除软故障状态。
static void FeetechStsControl_ClearSoftFault(void)
{
    // 清除可恢复故障并恢复空闲控制。
    if (g_claw_control_state.fault_level != FEETECH_STS_FAULT_LEVEL_HARD) {
        g_claw_control_state.fault_level = FEETECH_STS_FAULT_LEVEL_NONE;
    }

    if ((g_claw_control_state.result != FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE) &&
        (g_claw_control_state.result != FEETECH_STS_GRIP_RESULT_COMM_ERROR)) {
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
    }

    g_claw_control_state.overload_active = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;
}

// 目标角度分发到当前后端。
static HAL_StatusTypeDef FeetechStsControl_SendAngleToBackend(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    return FeetechStsControl_SendPositionToBackend(FeetechSts_AngleToPosition(angle_deg),
                                                   angle_deg,
                                                   time_ms,
                                                   speed,
                                                   acc);
}

// 目标位置仅下发到 STS 总线。
static HAL_StatusTypeDef FeetechStsControl_SendPositionToBackend(uint16_t position, float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc)
{
    (void)angle_deg;
    return FeetechStsBus_SetPositionEx(g_claw_control_state.servo_id,
                                       position,
                                       time_ms,
                                       speed,
                                       acc);
}
// 限制浮点范围。
static float FeetechStsControl_LimitFloatRange(float value, float min_value, float max_value)
{
    float tmp;

    if (min_value > max_value) {
        tmp = min_value;
        min_value = max_value;
        max_value = tmp;
    }

    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

// 加载持久化机械极限。
static void FeetechStsControl_LoadStoredLimits(void)
{
    ClawLimitStorageData_t data;

    g_claw_control_state.limits_valid = 0U;
    g_claw_control_state.stored_limit_sequence = 0UL;
    if ((ClawLimitStorage_Load(&data) == HAL_OK) &&
        (FeetechStsControl_AreLimitPositionsValid(data.open_position, data.close_position) != 0U)) {
        g_claw_manager_state.config.open_angle_deg = FeetechSts_PositionToAngle(data.open_position);
        g_claw_manager_state.config.close_angle_deg = FeetechSts_PositionToAngle(data.close_position);
        g_claw_control_state.limits_valid = 1U;
        g_claw_control_state.stored_limit_sequence = data.sequence;
    }
}

// 校验机械极限跨度。
static uint8_t FeetechStsControl_AreLimitPositionsValid(uint16_t open_position, uint16_t close_position)
{
    float open_angle;
    float close_angle;

    open_angle = FeetechSts_PositionToAngle(open_position);
    close_angle = FeetechSts_PositionToAngle(close_position);
    return (FeetechStsControl_AbsFloatDiff(open_angle, close_angle) >= FEETECH_STS_CONTROL_LIMIT_MIN_SPAN_DEG) ? 1U : 0U;
}

// 记录单侧极限；两侧齐全后校验并持久化。
static HAL_StatusTypeDef FeetechStsControl_RecordLimit(uint8_t record_open)
{
    const FeetechStsFeedback_t *feedback;
    HAL_StatusTypeDef status;

    if ((g_claw_control_state.limit_setting_active == 0U) ||
        (g_claw_control_state.sweep_active != 0U)) {
        return HAL_BUSY;
    }

    status = FeetechStsControl_RequestFeedback();
    feedback = FeetechStsBus_GetFeedback();
    if ((status != HAL_OK) || (feedback == 0) || (feedback->position_valid == 0U)) {
        return HAL_ERROR;
    }

    if (record_open != 0U) {
        g_claw_control_state.pending_open_position = feedback->position;
        g_claw_control_state.pending_open_valid = 1U;
    } else {
        g_claw_control_state.pending_close_position = feedback->position;
        g_claw_control_state.pending_close_valid = 1U;
    }
    return HAL_OK;
}

// 读取当前公共动作张开端点。
static float FeetechStsControl_GetMotionOpenAngle(void)
{
    if ((g_claw_control_state.limit_setting_active != 0U) ||
        (g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_FULL_270)) {
        return FEETECH_STS_CONTROL_B0_SWEEP_OPEN_ANGLE_DEG;
    }
    return g_claw_manager_state.config.open_angle_deg;
}

// 读取当前公共动作闭合端点。
static float FeetechStsControl_GetMotionCloseAngle(void)
{
    if ((g_claw_control_state.limit_setting_active != 0U) ||
        (g_claw_control_state.sweep_range == FEETECH_STS_SWEEP_RANGE_FULL_270)) {
        return FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG;
    }
    return g_claw_manager_state.config.close_angle_deg;
}

// 记录 B0 扫描打印事件。
static void FeetechStsControl_RecordSweepPrintEvent(const char *text, float angle_deg, float target_angle_deg)
{
    const char *event_text;

    event_text = (text != 0) ? text : "B0换向";
    strncpy(s_sweep_print_event.text, event_text, sizeof(s_sweep_print_event.text) - 1U);
    s_sweep_print_event.text[sizeof(s_sweep_print_event.text) - 1U] = '\0';
    s_sweep_print_event.angle_deg = angle_deg;
    s_sweep_print_event.target_angle_deg = target_angle_deg;
    s_sweep_print_event_ready = 1U;
}

// 刷新当前反馈指标。
static void FeetechStsControl_UpdatePresentMetric(void)
{
    // 从反馈缓存更新负载和电流快照。
    const FeetechStsFeedback_t *feedback;


    feedback = FeetechStsBus_GetFeedback();
    if (feedback == 0) {
        return;
    }

    g_claw_control_state.present_load_abs = FeetechStsControl_AbsS16(feedback->load);
    g_claw_control_state.present_current_ma = feedback->current_ma;
}

// 判断状态是否属于保持类。
static uint8_t FeetechStsControl_IsHoldLikeState(void)
{
    // 判断当前状态是否属于夹持保持类。
    if ((g_claw_control_state.state == FEETECH_STS_CONTROL_HOLDING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_GRIP_LOCKED) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_CARRY_HOLD) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_FAULT_HOLD)) {
        return 1U;
    }

    return 0U;
}

// 本地写入力矩限制并同步缓存。
static HAL_StatusTypeDef FeetechStsControl_SetTorqueLimitLocal(uint16_t limit)
{
    // 目标转矩写入 STS 总线。
    limit = FeetechStsControl_LimitTorque(limit);
    g_claw_control_state.target_torque_limit = limit;


    return FeetechStsBus_SetTorqueLimit(g_claw_control_state.servo_id, limit);
}


// 写入控制状态和状态时间。
static void FeetechStsControl_SetState(FeetechStsControlState_t state)
{
    // 切换控制状态并刷新状态时间戳。
    g_claw_control_state.state = state;
    g_claw_control_state.state_tick_ms = HAL_GetTick();

    if ((state != FEETECH_STS_CONTROL_FAULT_HOLD) &&
        (state != FEETECH_STS_CONTROL_ERROR) &&
        (g_claw_control_state.fault_level != FEETECH_STS_FAULT_LEVEL_HARD)) {
        g_claw_control_state.fault_level = FEETECH_STS_FAULT_LEVEL_NONE;
    }
}

// 清理已经结束的运动跟踪和扫描状态。
static void FeetechStsControl_ClearMotionRuntime(uint8_t clear_print_events)
{
    g_claw_control_state.sweep_active = 0U;
    g_claw_control_state.motion_track_active = 0U;
    g_claw_control_state.stall_last_feedback_tick_ms = 0UL;

    if (clear_print_events != 0U) {
        s_sweep_print_event_ready = 0U;
        s_motion_print_event_ready = 0U;
    }
}

// 写入最近夹取结果。
static void FeetechStsControl_SetResult(FeetechStsGripResult_t result)
{
    // 记录本次夹取结果。
    g_claw_control_state.result = result;
}

// 计算 S16 绝对值。
static uint16_t FeetechStsControl_AbsS16(int16_t value)
{
    // 返回 int16 数值的绝对值。
    if (value < 0) {
        return (uint16_t)(-value);
    }

    return (uint16_t)value;
}

// 计算浮点差绝对值。
static float FeetechStsControl_AbsFloatDiff(float a, float b)
{
    // 返回两个角度/浮点量的绝对差。
    if (a >= b) {
        return a - b;
    }

    return b - a;
}

// 限制力矩到安全范围。
static uint16_t FeetechStsControl_LimitTorque(uint16_t value)
{
    // 限制力矩值不超过舵机允许范围。
    if (value > FEETECH_STS_TORQUE_LIMIT_MAX) {
        return FEETECH_STS_TORQUE_LIMIT_MAX;
    }

    return value;
}

// 限制本地角度到机械范围。
static float FeetechStsControl_LimitLocalAngle(float angle_deg)
{
    // 限制角度在本机开合范围内。
    if (angle_deg < FEETECH_STS_ANGLE_MIN_DEG) {
        return FEETECH_STS_ANGLE_MIN_DEG;
    }

    if (angle_deg > FEETECH_STS_ANGLE_MAX_DEG) {
        return FEETECH_STS_ANGLE_MAX_DEG;
    }

    return angle_deg;
}

/*============================================================
 * 自适应与保护兼容区：默认关闭，后续删除从此处集中处理。
 *============================================================*/

// 切换夹取参数档位。
HAL_StatusTypeDef FeetechStsControl_SetProfile(FeetechStsGripProfile_t profile)
{
    // 切换夹取档位，并把档位参数写入运行配置。
    if (profile >= FEETECH_STS_GRIP_PROFILE_COUNT) {
        return HAL_ERROR;
    }

    g_claw_control_state.profile = profile;
    g_claw_manager_state.config.close_speed = s_grip_profiles[profile].close_speed;
    g_claw_manager_state.config.approach_torque_limit = s_grip_profiles[profile].grip_torque_limit;
    g_claw_manager_state.config.contact_torque_limit = s_grip_profiles[profile].grip_torque_limit;
    g_claw_manager_state.config.hold_torque_limit = s_grip_profiles[profile].hold_torque_limit;
    g_claw_manager_state.option.screen_update_request = 1U;
    return HAL_OK;
}

// 读取夹取档位配置。
const FeetechStsGripConfig_t *FeetechStsControl_GetProfileConfig(FeetechStsGripProfile_t profile)
{
    if (profile >= FEETECH_STS_GRIP_PROFILE_COUNT) {
        profile = FEETECH_STS_GRIP_PROFILE_MEDIUM;
    }

    return &s_grip_profiles[profile];
}

// 读取最近夹取结果。
FeetechStsGripResult_t FeetechStsControl_GetGripResult(void)
{
    return g_claw_control_state.result;
}

// 读取是否已检测到物体。
uint8_t FeetechStsControl_IsObjectDetected(void)
{
    // 读取物体夹持检测结果。
    return g_claw_control_state.object_detected;
}

// 取走 A0 夹取调试事件。
uint8_t FeetechStsControl_TakeGripDetectEvent(FeetechStsGripDetectEvent_t *out_event)
{
    // 事件流：控制层记录 -> 命令层打印 -> 清空标志。
    if ((out_event == 0) || (s_grip_detect_event_ready == 0U)) {
        return 0U;
    }

    *out_event = s_grip_detect_event;
    s_grip_detect_event_ready = 0U;
    return 1U;
}

// 读取夹取结果文本。
const char *FeetechStsControl_GetResultText(void)
{
    switch (g_claw_control_state.result) {
        case FEETECH_STS_GRIP_RESULT_OBJECT:
            return "OBJ";
        case FEETECH_STS_GRIP_RESULT_NO_OBJECT:
            return "NONE";
        case FEETECH_STS_GRIP_RESULT_TIMEOUT:
            return "TIME";
        case FEETECH_STS_GRIP_RESULT_OVERLOAD:
            return "OVLD";
        case FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE:
            return "LOWV";
        case FEETECH_STS_GRIP_RESULT_SLIP_PROTECT:
            return "SLIP";
        case FEETECH_STS_GRIP_RESULT_COMM_ERROR:
            return "COMM";
        case FEETECH_STS_GRIP_RESULT_NONE:
        default:
            return "----";
    }
}

#ifdef __ICCARM__
#pragma diag_suppress=Pe177
#endif
// 过载保护检测。
static void FeetechStsControl_TaskOverload(uint32_t now)
{
    // 过载保护：持续过载 -> 停止并进入故障。
    if ((g_claw_control_state.state != FEETECH_STS_CONTROL_CLOSING) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_GRIP_CONTACT) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_GRIP_SETTLE) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_GRIP_LOCKED) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_CARRY_HOLD) &&
        (g_claw_control_state.state != FEETECH_STS_CONTROL_HOLDING)) {
        g_claw_control_state.overload_active = 0U;
        return;
    }

    if (g_claw_control_state.present_load_abs < FEETECH_STS_CONTROL_GRIP_LOAD_ABS_LIMIT) {
        g_claw_control_state.overload_active = 0U;
        return;
    }

    if (g_claw_control_state.overload_active == 0U) {
        g_claw_control_state.overload_active = 1U;
        g_claw_control_state.overload_start_tick_ms = now;
        return;
    }

    if ((uint32_t)(now - g_claw_control_state.overload_start_tick_ms) >= FEETECH_STS_CONTROL_OVERLOAD_TIME_MS) {
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_OVERLOAD);
        g_claw_control_state.fault_level = FEETECH_STS_FAULT_LEVEL_SOFT;
        (void)FeetechStsControl_HoldCurrent();
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_FAULT_HOLD);
    }
}

// 低电压保护检测。
static void FeetechStsControl_TaskLowVoltage(uint32_t now)
{
    // 低压保护：低于阈值 -> 降级/故障处理。
    (void)now;

    if (FeetechStsControl_IsLowVoltageActive() == 0U) {
        g_claw_control_state.low_voltage_active = 0U;
        return;
    }

    g_claw_control_state.low_voltage_active = 1U;

    if ((g_claw_control_state.state == FEETECH_STS_CONTROL_SWEEP) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_CLOSING) ||
        (g_claw_control_state.state == FEETECH_STS_CONTROL_OPENING)) {
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE);
        g_claw_control_state.fault_level = FEETECH_STS_FAULT_LEVEL_HARD;
        g_claw_control_state.owner = FEETECH_STS_OWNER_SAFETY;
        g_claw_control_state.control_mode = FEETECH_STS_CONTROL_MODE_SAFE_HOLD;
        (void)FeetechStsControl_HoldCurrent();
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_FAULT_HOLD);
    }
}
#ifdef __ICCARM__
#pragma diag_default=Pe177
#endif

// A0/B0 观察反馈，确认自适应夹取。
static void FeetechStsControl_TaskAdaptiveGrip(uint32_t now)
{
    // A0 闭合、B0 扫描全程：阈值 + 角度停滞连续确认。
    const FeetechStsFeedback_t *feedback;
    const FeetechStsGripConfig_t *cfg;
    const char *grip_reason;
    uint16_t load_abs;
    uint16_t current_ma;
    uint32_t elapsed;
    uint8_t load_hit;
    uint8_t current_hit;
    uint8_t threshold_hit;
    uint8_t angle_stable;
    uint8_t confirm_need;
    uint8_t is_a0_closing;
    uint8_t is_b0_sweep;

    is_a0_closing = (g_claw_control_state.state == FEETECH_STS_CONTROL_CLOSING) ? 1U : 0U;
    is_b0_sweep = ((g_claw_control_state.state == FEETECH_STS_CONTROL_SWEEP) &&
                   (g_claw_control_state.sweep_active != 0U)) ? 1U : 0U;
    if ((is_a0_closing == 0U) && (is_b0_sweep == 0U)) {
        return;
    }

    feedback = FeetechStsBus_GetFeedback();
    cfg = FeetechStsControl_GetProfileConfig(g_claw_control_state.profile);
    elapsed = (uint32_t)(now - g_claw_control_state.state_tick_ms);

    // 关闭自适应：固定目标闭合；到端点或超时后仅结束状态判断，目标和最大转矩保持不变。
    if (s_adaptive_grip_enable == 0U) {
        if ((is_a0_closing != 0U) &&
            ((((feedback != 0) && (feedback->position_valid != 0U) &&
               (feedback->moving == 0U) &&
               (FeetechStsControl_IsNearCloseLimit(feedback, cfg) != 0U))) ||
             (elapsed >= cfg->timeout_ms))) {
              g_claw_control_state.object_detected = 0U;
              FeetechStsControl_ClearMotionRuntime(0U);

              FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_NONE);
              (void)FeetechStsControl_SetTorqueLimitLocal(
                  g_claw_manager_state.config.full_torque_limit);

              FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
        }
        return;
    }


    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        if ((is_a0_closing != 0U) && (elapsed >= cfg->timeout_ms)) {
            FeetechStsControl_HandleNoObject(FEETECH_STS_GRIP_RESULT_TIMEOUT);
        }
        return;
    }

    if (feedback->rx_packet_count <= g_claw_control_state.grip_last_rx_packet_count) {
        if ((is_a0_closing != 0U) && (elapsed >= cfg->timeout_ms)) {
            FeetechStsControl_HandleNoObject(FEETECH_STS_GRIP_RESULT_TIMEOUT);
        }
        return;
    }
    g_claw_control_state.grip_last_rx_packet_count = feedback->rx_packet_count;

    if (elapsed < cfg->startup_ignore_ms) {
        FeetechStsControl_ResetGripStableState(feedback);
        return;
    }

    load_abs = (g_claw_manager_state.feedback.load_abs_filtered != 0U) ? g_claw_manager_state.feedback.load_abs_filtered : FeetechStsControl_AbsS16(feedback->load);
    current_ma = (g_claw_manager_state.feedback.current_ma_filtered != 0U) ? g_claw_manager_state.feedback.current_ma_filtered : feedback->current_ma;
    load_hit = (load_abs >= cfg->load_threshold) ? 1U : 0U;
    current_hit = ((cfg->current_threshold_ma != 0U) && (current_ma >= cfg->current_threshold_ma)) ? 1U : 0U;
    threshold_hit = ((load_hit != 0U) || (current_hit != 0U)) ? 1U : 0U;
    angle_stable = FeetechStsControl_IsGripAngleStable(feedback);

    if ((is_a0_closing != 0U) &&
        (FeetechStsControl_IsNearCloseLimit(feedback, cfg) != 0U) &&
        (threshold_hit == 0U) &&
        (feedback->moving == 0U)) {
        FeetechStsControl_HandleNoObject(FEETECH_STS_GRIP_RESULT_NO_OBJECT);
        return;
    }

    if ((threshold_hit != 0U) && (angle_stable != 0U)) {
        if (g_claw_control_state.grip_trigger_counter < 255U) {
            g_claw_control_state.grip_trigger_counter++;
        }

        grip_reason = "THRESHOLD_STALL";

        FeetechStsControl_RecordGripDetectEvent(grip_reason,
                                                feedback,
                                                g_claw_control_state.grip_trigger_counter);

        confirm_need = (cfg->trigger_count == 0U) ? 1U : cfg->trigger_count;
        if (g_claw_control_state.grip_trigger_counter >= confirm_need) {
            FeetechStsControl_HandleGripSuccess();
        }
        return;
    }

    g_claw_control_state.grip_trigger_counter = 0U;

    if ((is_a0_closing != 0U) && (elapsed >= cfg->timeout_ms)) {
        FeetechStsControl_HandleNoObject(FEETECH_STS_GRIP_RESULT_TIMEOUT);
    }
}

// 重置 A0 角度停滞判断基准。
static void FeetechStsControl_ResetGripStableState(const FeetechStsFeedback_t *feedback)
{
    // 当前反馈角度 -> 下一次停滞判断基准。
    if ((feedback != 0) && (feedback->position_valid != 0U)) {
        g_claw_control_state.grip_last_angle_deg = feedback->angle_deg;
    } else {
        g_claw_control_state.grip_last_angle_deg = g_claw_control_state.target_angle_deg;
    }

    g_claw_control_state.grip_delta_last_deg = 0.0f;
    g_claw_control_state.grip_trigger_counter = 0U;
}

// 判断 A0 本次反馈角度是否基本不变。
static uint8_t FeetechStsControl_IsGripAngleStable(const FeetechStsFeedback_t *feedback)
{
    // 与上一包反馈比较；moving 只打印，不参与成功判定。
    float delta;

    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        return 0U;
    }

    delta = FeetechStsControl_AbsFloatDiff(feedback->angle_deg, g_claw_control_state.grip_last_angle_deg);
    g_claw_control_state.grip_delta_last_deg = delta;
    g_claw_control_state.grip_last_angle_deg = feedback->angle_deg;

    return (delta <= FEETECH_STS_CONTROL_A0_STABLE_DELTA_DEG) ? 1U : 0U;
}

// 记录 A0 夹取确认调试现场。
static void FeetechStsControl_RecordGripDetectEvent(const char *reason, const FeetechStsFeedback_t *feedback, uint8_t confirm_count)
{
    // 现场量：原因、A、TG、角度变化、阈值、负载、电流、moving。
    uint16_t load_abs;
    uint16_t current_ma;

    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        return;
    }

    load_abs = (g_claw_manager_state.feedback.load_abs_filtered != 0U) ? g_claw_manager_state.feedback.load_abs_filtered : FeetechStsControl_AbsS16(feedback->load);
    current_ma = (g_claw_manager_state.feedback.current_ma_filtered != 0U) ? g_claw_manager_state.feedback.current_ma_filtered : feedback->current_ma;

    s_grip_detect_event.reason = (reason != 0) ? reason : "UNKNOWN";
    s_grip_detect_event.angle_deg = feedback->angle_deg;
    s_grip_detect_event.target_angle_deg = g_claw_control_state.target_angle_deg;
    s_grip_detect_event.delta_last_deg = g_claw_control_state.grip_delta_last_deg;
    s_grip_detect_event.delta_limit_deg = FEETECH_STS_CONTROL_A0_STABLE_DELTA_DEG;
    s_grip_detect_event.load_abs = load_abs;
    s_grip_detect_event.current_ma = current_ma;
    s_grip_detect_event.confirm_count = confirm_count;
    s_grip_detect_event.moving = feedback->moving;
    s_grip_detect_event_ready = 1U;
}

// 判断是否接近闭合极限。
static uint8_t FeetechStsControl_IsNearCloseLimit(const FeetechStsFeedback_t *feedback, const FeetechStsGripConfig_t *cfg)
{
    // 判断当前位置是否接近闭合极限。
    float diff;

    if ((feedback == 0) || (cfg == 0) || (feedback->position_valid == 0U)) {
        return 0U;
    }

    if (g_claw_manager_state.config.close_angle_deg >= g_claw_manager_state.config.open_angle_deg) {
        diff = g_claw_manager_state.config.close_angle_deg - feedback->angle_deg;
    } else {
        diff = feedback->angle_deg - g_claw_manager_state.config.close_angle_deg;
    }

    if (diff < 0.0f) {
        diff = -diff;
    }

    return (diff <= cfg->close_limit_margin_deg) ? 1U : 0U;
}

// 夹取成功后转入当前位置保持。
static void FeetechStsControl_HandleGripSuccess(void)
{
    // 当前角度 -> 保持目标；保持转矩 -> 不卸力。
    HAL_StatusTypeDef status;

    g_claw_control_state.object_detected = 1U;
    s_open_close_closed = 1U;
    g_claw_control_state.last_motion_direction_close = 1U;
    FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_OBJECT);

    status = FeetechStsControl_HoldCurrent();
    if (status != HAL_OK) {
        return;
    }

#if (FEETECH_STS_CONTROL_ADAPTIVE_GRIP_ENABLE != 0U)
    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.hold_torque_limit);
#else
    status = FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.full_torque_limit);
#endif
    if (status != HAL_OK) {
        g_claw_control_state.last_status = status;
        FeetechStsControl_SetState(FEETECH_STS_CONTROL_ERROR);
        FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_COMM_ERROR);
        return;
    }

    FeetechStsControl_SetState(FEETECH_STS_CONTROL_HOLDING);
}

// 未夹到物体后的收尾处理。
static void FeetechStsControl_HandleNoObject(
    FeetechStsGripResult_t result)
{
    g_claw_control_state.object_detected = 0U;
    g_claw_control_state.grip_trigger_counter = 0U;

    FeetechStsControl_ClearMotionRuntime(1U);

    FeetechStsControl_SetResult(result);
    FeetechStsControl_SetState(FEETECH_STS_CONTROL_NO_OBJECT);

#if (FEETECH_STS_CONTROL_NO_OBJECT_OPEN != 0U)
    (void)FeetechStsControl_Release();
#endif
}

// 持物后滑移补偿。
static void FeetechStsControl_TaskSlipProtect(uint32_t now)
{
    // 防滑保护：保持态检测滑移并补偿目标角。
    const FeetechStsFeedback_t *feedback;
    float angle_diff;
    uint16_t load_drop;
    float next_angle;


    if ((FeetechStsControl_IsHoldLikeState() == 0U) ||
        (g_claw_control_state.object_detected == 0U)) {
        return;
    }

    if ((uint32_t)(now - g_claw_control_state.last_slip_compensate_tick_ms) < g_claw_manager_state.config.slip_compensate_period_ms) {
        return;
    }

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback == 0) || (feedback->position_valid == 0U)) {
        return;
    }

    angle_diff = FeetechStsControl_AbsFloatDiff(feedback->angle_deg, g_claw_control_state.hold_angle_deg);
    if (g_claw_control_state.hold_load_abs > g_claw_control_state.present_load_abs) {
        load_drop = (uint16_t)(g_claw_control_state.hold_load_abs - g_claw_control_state.present_load_abs);
    } else {
        load_drop = 0U;
    }

    if ((angle_diff < g_claw_manager_state.config.slip_angle_deg) && (load_drop < g_claw_manager_state.config.slip_load_drop)) {
        return;
    }

    next_angle = g_claw_control_state.target_angle_deg;
    if (g_claw_manager_state.config.close_angle_deg >= g_claw_manager_state.config.open_angle_deg) {
        next_angle += g_claw_manager_state.config.slip_compensate_deg;
        if (next_angle > g_claw_manager_state.config.close_angle_deg) {
            next_angle = g_claw_manager_state.config.close_angle_deg;
        }
    } else {
        next_angle -= g_claw_manager_state.config.slip_compensate_deg;
        if (next_angle < g_claw_manager_state.config.close_angle_deg) {
            next_angle = g_claw_manager_state.config.close_angle_deg;
        }
    }

    g_claw_control_state.last_slip_compensate_tick_ms = now;
    if (g_claw_control_state.slip_compensate_count < 255U) {
        g_claw_control_state.slip_compensate_count++;
    }
    FeetechStsControl_SetResult(FEETECH_STS_GRIP_RESULT_SLIP_PROTECT);
    (void)FeetechStsControl_SetTorqueLimitLocal(g_claw_manager_state.config.contact_torque_limit);
    (void)FeetechStsControl_MoveToAngle(next_angle, g_claw_manager_state.config.default_move_time_ms);
}

// 判断低电压保护是否有效。
static uint8_t FeetechStsControl_IsLowVoltageActive(void)
{
    // 根据反馈电压判断低压是否有效。
    const FeetechStsFeedback_t *feedback;

    if (g_claw_manager_state.config.low_voltage_mv == 0U) {
        return 0U;
    }

    feedback = FeetechStsBus_GetFeedback();
    if ((feedback == 0) || (feedback->telemetry_valid == 0U) || (feedback->voltage_mv == 0U)) {
        return 0U;
    }

    return (feedback->voltage_mv < g_claw_manager_state.config.low_voltage_mv) ? 1U : 0U;
}
