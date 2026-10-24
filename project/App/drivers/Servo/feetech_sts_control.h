/*********************************************************************************************************************
 * @file    feetech_sts_control.h
 * @brief   机械爪本地动作控制接口。
 *
 * @details
 * 1. 本文件负责：定义动作参数、状态、管理结构和对外控制函数。
 * 2. 本文件不负责：声明底层 UART 帧解析细节。
 * 3. 主要流程：配置/运行状态分离，外部通过接口读写。
 *********************************************************************************************************************/

#ifndef __FEETECH_STS_CONTROL_H__
#define __FEETECH_STS_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include "feetech_sts_bus.h"
#include "feature_config.h"
#include <stdint.h>

/*============================================================
 * 1. 可移植配置宏
 *============================================================*/

#define FEETECH_STS_CONTROL_SERVO_ID              FEETECH_STS_DEFAULT_ID     // 当前控制舵机 ID

#define FEETECH_STS_CONTROL_OPEN_ANGLE_DEG        FEETECH_STS_CONTROL_B0_SWEEP_OPEN_ANGLE_DEG  // 张开端点角度
#define FEETECH_STS_CONTROL_CLOSE_ANGLE_DEG       FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG // 闭合端点角度
#define FEETECH_STS_CONTROL_MOVE_TIME_MS          100U                       // 普通动作时间
#define FEETECH_STS_CONTROL_RELEASE_TIME_MS       130U                       // A0 双击张开时间
#define FEETECH_STS_CONTROL_GRIP_TIME_MS          0U                         // A0 快速夹取时间，0=主要按速度执行

#define FEETECH_STS_CONTROL_A0_DOUBLE_CLICK_MS    350U                       // A0 单击/双击确认窗口
#define FEETECH_STS_CONTROL_LIMIT_MIN_SPAN_DEG    3.0f                       // 机械极限最小有效跨度
#define FEETECH_STS_CONTROL_A0_CLOSE_SPEED        FEETECH_STS_CONTROL_B0_SWEEP_SPEED // A0 夹取速度
#define FEETECH_STS_CONTROL_A0_OPEN_SPEED         FEETECH_STS_CONTROL_B0_SWEEP_SPEED // A0 张开速度
#define FEETECH_STS_CONTROL_A0_FAST_ACC           FEETECH_STS_DEFAULT_ACC    // A0 快速夹取加速度
#define FEETECH_STS_CONTROL_A0_OPEN_ACC           FEETECH_STS_DEFAULT_ACC    // A0 张开加速度

#define FEETECH_STS_CONTROL_B0_SWEEP_OPEN_ANGLE_DEG   0.0f                   // B0 扫描最小角度
#define FEETECH_STS_CONTROL_B0_SWEEP_CLOSE_ANGLE_DEG  270.0f                 // B0 扫描最大角度
#define FEETECH_STS_CONTROL_B0_SWEEP_SPEED            1000U                  // B0 扫描速度
#define FEETECH_STS_CONTROL_B0_SWEEP_ACC              FEETECH_STS_DEFAULT_ACC // B0 扫描加速度
#define FEETECH_STS_CONTROL_SWEEP_TIME_MS             200U                   // B0 单程兜底换向时间
#define FEETECH_STS_CONTROL_SWEEP_HOLD_MS             60U                    // B0 端点换向等待时间
#define FEETECH_STS_CONTROL_CALIBRATION_SWEEP_SPEED     CLAW_DEFAULT_CALIBRATION_SWEEP_SPEED
#define FEETECH_STS_CONTROL_STALL_ENABLE              CLAW_FEATURE_STALL_STOP_ENABLE
#define FEETECH_STS_CONTROL_STALL_STARTUP_MS          CLAW_DEFAULT_STALL_STARTUP_MS
#define FEETECH_STS_CONTROL_STALL_TIMEOUT_MS          CLAW_DEFAULT_STALL_TIMEOUT_MS
#define FEETECH_STS_CONTROL_STALL_MIN_DELTA_POSITION  CLAW_DEFAULT_STALL_MIN_DELTA_POSITION
#define FEETECH_STS_CONTROL_STALL_TARGET_MARGIN       CLAW_DEFAULT_STALL_TARGET_MARGIN_POSITION

#define FEETECH_STS_CONTROL_FEEDBACK_PERIOD_MS    5U                         // 夹取状态反馈读取周期
#define FEETECH_STS_CONTROL_IDLE_FEEDBACK_MS      10U                        // 非夹取状态反馈读取周期
#define FEETECH_STS_CONTROL_FEEDBACK_RETRY_MS     100U                       // 反馈失败后的重试周期
#define FEETECH_STS_CONTROL_FEEDBACK_FRESH_MS     200U                       // 停止动作允许使用的反馈最大年龄
#define FEETECH_STS_CONTROL_STOP_LEAD_POSITION     10U                         // 停止时沿运动方向前补编码。
#define FEETECH_STS_CONTROL_DEFAULT_SPEED         800U                       // 普通动作默认速度
#define FEETECH_STS_CONTROL_DEFAULT_ACC           FEETECH_STS_DEFAULT_ACC    // 普通动作默认加速度

#define FEETECH_STS_CONTROL_GRIP_FULL_TORQUE      1000U                      // 夹取/释放前恢复扭矩限制

#define FEETECH_STS_CONTROL_SWEEP_STEP_DEG        5.0f                       // 保留：扫描步进角度
#define FEETECH_STS_CONTROL_SWEEP_STEP_PERIOD_MS  40U                        // 保留：扫描步进周期

#define FEETECH_STS_MANAGER_PRINT_DEFAULT         CLAW_DEFAULT_AUTO_PRINT_ENABLE // 自动状态打印默认开关
#define FEETECH_STS_MANAGER_PRINT_PERIOD_MS       CLAW_DEFAULT_AUTO_PRINT_PERIOD_MS // 自动状态打印周期


#define FEETECH_STS_CONTROL_EXTERNAL_TIMEOUT_MS   3000U                      // 外部控制超时接管时间
#define FEETECH_STS_CONTROL_PASS_FEEDBACK_MS      100U                       // 透传/手动模式反馈读取周期


/* 自适应与保护配置：默认关闭，集中放置便于后续移除。 */
#define FEETECH_STS_CONTROL_A0_STABLE_DELTA_DEG  0.5f                    // A0 角度基本不变阈值
#define FEETECH_STS_CONTROL_ADAPTIVE_GRIP_ENABLE  CLAW_FEATURE_ADAPTIVE_GRIP_ENABLE // 1=自适应夹取；0=位置模式强制闭合
#define FEETECH_STS_CONTROL_GRIP_LOAD_ABS_LIMIT   850U                       // 过载保护负载阈值
#define FEETECH_STS_CONTROL_OVERLOAD_TIME_MS      1000U                      // 过载持续判定时间
#define FEETECH_STS_CONTROL_NO_OBJECT_OPEN        0U                         // 空夹后自动张开开关
#define FEETECH_STS_CONTROL_GRIP_MIN_VALID_MOVE_DEG 5.0f                     // 旧参数兼容，A0 新逻辑不用
#define FEETECH_STS_CONTROL_GRIP_CONTACT_MS       200U                       // 接触确认时间
#define FEETECH_STS_CONTROL_GRIP_SETTLE_MS        300U                       // 夹取稳定确认时间
#define FEETECH_STS_CONTROL_LOW_VOLTAGE_MV        0U                         // 低电压保护阈值，0=关闭
#define FEETECH_STS_CONTROL_SLIP_ANGLE_DEG        5.0f                       // 防滑角度阈值
#define FEETECH_STS_CONTROL_SLIP_LOAD_DROP        80U                        // 防滑负载下降阈值
#define FEETECH_STS_CONTROL_SLIP_COMPENSATE_DEG   3.0f                       // 防滑补夹角度
#define FEETECH_STS_CONTROL_SLIP_COMPENSATE_PERIOD_MS 400U                   // 防滑补夹最小间隔
#define FEETECH_STS_CONTROL_SLIP_PROTECT_ENABLE   CLAW_FEATURE_SLIP_PROTECT_ENABLE // 1=启用防滑补夹；0=纯位置保持

/*============================================================
 * 2. 状态和参数定义
 *============================================================*/


typedef enum {
    FEETECH_STS_SWEEP_RANGE_LIMITS = 0,           /* 使用已保存机械极限。 */
    FEETECH_STS_SWEEP_RANGE_FULL_270              /* 开发者范围：0~270 度。 */
} FeetechStsSweepRange_t;

typedef enum {
    FEETECH_STS_CONTROL_IDLE = 0,                 /* 空闲。 */
    FEETECH_STS_CONTROL_OPENING,                  /* 正在张开。 */
    FEETECH_STS_CONTROL_CLOSING,                  /* 固定速度闭合夹取中。 */
    FEETECH_STS_CONTROL_GRIP_CONTACT,             /* 已检测到接触，等待短时确认。 */
    FEETECH_STS_CONTROL_GRIP_SETTLE,              /* 夹取稳定确认中，用于无人机挂载前确认不滑动。 */
    FEETECH_STS_CONTROL_GRIP_LOCKED,              /* 已确认夹住，可进入空中搬运保持。 */
    FEETECH_STS_CONTROL_CARRY_HOLD,               /* 空中搬运保持状态，持续做防滑和低压保护。 */
    FEETECH_STS_CONTROL_HOLDING,                  /* 已夹住，当前位置保持。 */
    FEETECH_STS_CONTROL_RELEASE_ARMED,            /* 释放已授权，等待最终确认。 */
    FEETECH_STS_CONTROL_RELEASING,                /* 正在释放。 */
    FEETECH_STS_CONTROL_SWEEP,                    /* 往复测试中。 */
    FEETECH_STS_CONTROL_GRIP_STEP_CONFIRM,        /* 旧状态兼容，A0 新逻辑不进入。 */
    FEETECH_STS_CONTROL_NO_OBJECT,                /* 闭合到位或超时但未检测到物体。 */
    FEETECH_STS_CONTROL_FAULT_HOLD,               /* 故障保持，不主动松开，等待人工确认。 */
    FEETECH_STS_CONTROL_ERROR,                    /* 通信或安全保护错误。 */
    FEETECH_STS_CONTROL_LIMIT_SETTING             /* 机械极限标定中。 */
} FeetechStsControlState_t;

typedef enum {
    FEETECH_STS_GRIP_PROFILE_SOFT = 0,            /* 软质物体。 */
    FEETECH_STS_GRIP_PROFILE_MEDIUM,              /* 中等硬度物体。 */
    FEETECH_STS_GRIP_PROFILE_HARD,                /* 硬质物体。 */
    FEETECH_STS_GRIP_PROFILE_FRAGILE,             /* 易碎物体。 */
    FEETECH_STS_GRIP_PROFILE_COUNT
} FeetechStsGripProfile_t;

typedef enum {
    FEETECH_STS_GRIP_RESULT_NONE = 0,             /* 尚无结果。 */
    FEETECH_STS_GRIP_RESULT_OBJECT,               /* 已夹到物体。 */
    FEETECH_STS_GRIP_RESULT_NO_OBJECT,            /* 未夹到物体。 */
    FEETECH_STS_GRIP_RESULT_TIMEOUT,              /* 夹取超时。 */
    FEETECH_STS_GRIP_RESULT_OVERLOAD,             /* 过载保护。 */
    FEETECH_STS_GRIP_RESULT_LOW_VOLTAGE,          /* 低电压保护。 */
    FEETECH_STS_GRIP_RESULT_SLIP_PROTECT,         /* 检测到滑动并执行补夹。 */
    FEETECH_STS_GRIP_RESULT_COMM_ERROR            /* 通信/发送错误。 */
} FeetechStsGripResult_t;

typedef enum {
    FEETECH_STS_OWNER_NONE = 0,                    /* 无控制者。 */
    FEETECH_STS_OWNER_KEY_A0,                      /* A0 按键控制。 */
    FEETECH_STS_OWNER_KEY_B0,                      /* B0 按键控制。 */
    FEETECH_STS_OWNER_VOFA,                        /* VOFA/USB-TTL 控制。 */
    FEETECH_STS_OWNER_BLUETOOTH,                   /* 蓝牙控制。 */
    FEETECH_STS_OWNER_USB_DIRECT,                  /* 电脑/飞特官方帧直控。 */
    FEETECH_STS_OWNER_SAFETY,                      /* 安全保护控制。 */
    FEETECH_STS_OWNER_SCREEN                       /* 本机菜单控制；追加以保留旧编号。 */
} FeetechStsControlOwner_t;

typedef enum {
    FEETECH_STS_CONTROL_MODE_AUTO = 0,             /* 自动模式：本机按键和状态机可以写目标。 */
    FEETECH_STS_CONTROL_MODE_MANUAL,               /* 外部控制帧接管目标，本机继续反馈/显示。 */
    FEETECH_STS_CONTROL_MODE_PASS_THROUGH,         /* 透传模式，外部官方帧直通；新控制命令可覆盖。 */
    FEETECH_STS_CONTROL_MODE_SAFE_HOLD             /* 安全保持：只允许安全动作。 */
} FeetechStsControlMode_t;

typedef enum {
    FEETECH_STS_FAULT_LEVEL_NONE = 0,              /* 无故障。 */
    FEETECH_STS_FAULT_LEVEL_SOFT,                  /* 软故障，允许按键清除后继续动作。 */
    FEETECH_STS_FAULT_LEVEL_HARD                   /* 硬故障，低压/通信等禁止继续动作。 */
} FeetechStsFaultLevel_t;

typedef struct {
    uint16_t close_speed;                          /* 闭合夹取速度，固定速度，不随接近闭合而减速。 */
    uint16_t load_threshold;                       /* 当前负载绝对值阈值，0~1000。 */
    uint16_t current_threshold_ma;                 /* 当前电流阈值，单位 mA；0 表示不使用电流独立触发。 */
    uint16_t grip_torque_limit;                    /* 夹取运动阶段转矩限制，0~1000。 */
    uint16_t hold_torque_limit;                    /* 夹住后保持转矩限制，0~1000。 */
    uint8_t trigger_count;                         /* 连续满足阈值次数。 */
    uint16_t startup_ignore_ms;                    /* 起步屏蔽时间，避免启动冲击误判。 */
    uint16_t timeout_ms;                           /* 最大夹取时间。 */
    float close_limit_margin_deg;                  /* 闭合末端限位判断范围；只用于空夹判定，不用于减速。 */
} FeetechStsGripConfig_t;

typedef struct {
    FeetechStsControlState_t state;                /* 当前动作状态。 */
    FeetechStsGripProfile_t profile;               /* 当前夹取档位。 */
    FeetechStsGripResult_t result;                 /* 最近一次夹取结果。 */
    uint8_t servo_id;                              /* 当前控制的舵机 ID。 */
    uint8_t object_detected;                       /* 1：已判定夹到物体。 */
    uint8_t sweep_active;                          /* 1：往复动作正在运行。 */
    uint8_t sweep_direction_close;                 /* 1：下一段向合爪角运动；0：下一段向开爪角运动。 */
    uint8_t last_motion_direction_close;             /* 最近有效运动方向：1 闭合，0 张开。 */
    uint8_t grip_trigger_counter;                  /* 连续超过阈值计数。 */
    uint8_t overload_active;                       /* 1：过载计时中。 */
    uint8_t limit_setting_active;                  /* 1：机械极限标定中。 */
    uint8_t limits_valid;                          /* 1：存在有效的持久化机械极限。 */
    uint8_t pending_open_valid;                    /* 1：本轮已记录张开极限。 */
    uint8_t pending_close_valid;                   /* 1：本轮已记录闭合极限。 */
    FeetechStsSweepRange_t sweep_range;            /* A0/B0 当前公共动作范围。 */
    uint16_t pending_open_position;                /* 本轮张开极限原始位置。 */
    uint16_t pending_close_position;               /* 本轮闭合极限原始位置。 */
    uint32_t stored_limit_sequence;                /* 当前持久化记录序号。 */

    float target_angle_deg;                        /* 当前目标角度。 */
    uint16_t target_position;                      /* 当前目标位置值。 */
    uint16_t target_time_ms;                       /* 当前动作时间。 */
    uint16_t target_speed;                         /* 当前动作速度。 */
    uint16_t target_torque_limit;                  /* 当前目标转矩限制。 */

    uint16_t present_load_abs;                     /* 当前负载绝对值。 */
    uint16_t present_current_ma;                   /* 当前电流，单位 mA。 */
    uint16_t hold_position;                        /* 夹住后保持的位置。 */
    float hold_angle_deg;                          /* 夹住后记录的保持角度，用于防滑检测。 */
    uint16_t hold_load_abs;                        /* 夹住后记录的负载绝对值，用于负载下降防滑判断。 */

    uint8_t low_voltage_active;                    /* 1：当前处于低电压保护。 */
    uint8_t release_permission;                    /* 1：允许释放；0：禁止释放。 */
    uint8_t release_armed;                         /* 1：释放已授权，等待确认。 */
    uint8_t slip_compensate_count;                 /* 防滑补夹次数。 */

    FeetechStsControlOwner_t owner;                /* 当前写控制权来源。 */
    FeetechStsControlMode_t control_mode;          /* 当前控制模式。 */
    FeetechStsFaultLevel_t fault_level;            /* 当前故障等级。 */
    uint8_t local_write_enable;                    /* 1：最新命令来自按键，允许本地状态机续写目标。 */
    uint32_t last_external_write_tick_ms;          /* 最近一次外部写帧时间。 */
    uint32_t owner_timeout_ms;                     /* 当前控制权超时时间。 */

    float grip_last_angle_deg;                     /* 上一次反馈角度，用于停滞判断。 */
    float grip_delta_last_deg;                     /* 最近一次反馈角度变化。 */

    HAL_StatusTypeDef last_status;                 /* 最近一次底层发送状态。 */
    uint32_t state_tick_ms;                        /* 最近一次状态切换时间。 */
    uint32_t last_feedback_tick_ms;                /* 最近一次主动读取反馈时间。 */
    uint32_t overload_start_tick_ms;               /* 过载开始计时时间。 */
    uint32_t last_slip_compensate_tick_ms;         /* 最近一次防滑补夹时间。 */
    uint32_t grip_last_rx_packet_count;            /* 最近已处理的反馈包计数。 */

    uint8_t motion_track_active;                   /* 1：当前目标尚在跟踪。 */
    uint16_t stall_reference_position;             /* 卡死判断参考编码。 */
    uint32_t motion_start_tick_ms;                 /* 当前运动开始时间。 */
    uint32_t stall_reference_tick_ms;              /* 最近有效位移时间。 */
    uint32_t stall_last_feedback_tick_ms;          /* 最近已判断反馈时间。 */
} FeetechStsControlStatus_t;

typedef struct {
    const char *reason;                           /* 触发原因，命令层转中文打印。 */
    float angle_deg;                              /* 确认时实际角度。 */
    float target_angle_deg;                       /* 确认时目标角度。 */
    float delta_last_deg;                         /* 本次反馈角度变化。 */
    float delta_limit_deg;                        /* 角度停滞阈值。 */
    uint16_t load_abs;                            /* 确认时负载绝对值。 */
    uint16_t current_ma;                          /* 确认时电流。 */
    uint8_t confirm_count;                        /* 连续确认次数。 */
    uint8_t moving;                               /* moving 仅用于辅助观察。 */
} FeetechStsGripDetectEvent_t;

typedef struct {
    char text[16];                                /* 打印文本副本。 */
    float angle_deg;                              /* 当前角度。 */
    float target_angle_deg;                       /* 目标角度。 */
} FeetechStsSweepPrintEvent_t;

typedef struct {
    uint8_t stalled;                              /* 1：卡死停止；0：正常到位。 */
    float angle_deg;                              /* 触发时实际角度。 */
    uint16_t position;                            /* 触发时编码器值。 */
    uint16_t target_position;                     /* 目标编码。 */
} FeetechStsMotionPrintEvent_t;


/*============================================================
 * 统一信息管理结构体
 *============================================================*/

typedef struct {
    uint8_t servo_id;                               /* 当前控制舵机 ID。 */
    float open_angle_deg;                          /* 张开基准角度，单位：度。 */
    float close_angle_deg;                         /* 闭合基准角度，单位：度。 */

    uint16_t default_move_time_ms;                 /* 普通动作默认时间，单位：ms。 */
    uint16_t default_release_time_ms;              /* 释放动作默认时间，单位：ms。 */
    uint16_t default_sweep_time_ms;                /* 扫描动作参考时间，单位：ms。 */
    uint16_t default_grip_time_ms;                 /* 夹取动作默认时间，0 表示按速度闭合。 */

    uint16_t default_speed;                        /* 普通动作默认速度，后续动作未指定速度时以此为基准。 */
    uint16_t open_speed;                           /* 张开动作默认速度。 */
    uint16_t close_speed;                          /* 普通闭合/夹取默认速度。 */
    uint16_t sweep_speed;                          /* B0 扫描默认速度。 */
    uint16_t calibration_sweep_speed;              /* 极限标定扫描速度。 */
    uint8_t default_acc;                           /* 普通动作默认加速度。 */

    uint16_t full_torque_limit;                    /* 夹取/释放前恢复的默认最大转矩限制。 */
    uint16_t approach_torque_limit;                /* 夹取接近阶段转矩限制，用于降低闭合冲击。 */
    uint16_t contact_torque_limit;                 /* 接触确认阶段转矩限制，用于稳定接触。 */
    uint16_t hold_torque_limit;                    /* 默认保持转矩限制。 */

    uint16_t feedback_period_ms;                   /* 夹取/保持状态反馈读取周期。 */
    uint16_t idle_feedback_period_ms;              /* 非夹取状态反馈读取周期。 */
    uint16_t grip_feedback_period_ms;              /* 夹取状态反馈读取周期，便于后续单独调节。 */

    float grip_min_valid_move_deg;                 /* 旧参数兼容，A0 新逻辑不使用。 */
    uint16_t grip_contact_ms;                      /* 接触确认阶段时间。 */
    uint16_t grip_settle_ms;                       /* 夹取稳定确认阶段时间。 */

    float sweep_step_deg;                          /* B0 扫描步进角度。 */
    uint16_t sweep_step_period_ms;                 /* B0 扫描步进周期。 */

    uint16_t low_voltage_mv;                       /* 低电压保护阈值，单位 mV，0 表示关闭。 */
    float slip_angle_deg;                          /* 保持状态滑动角度阈值。 */
    uint16_t slip_load_drop;                       /* 保持状态负载下降阈值。 */
    float slip_compensate_deg;                     /* 防滑补夹角度。 */
    uint16_t slip_compensate_period_ms;            /* 防滑补夹最小间隔。 */

    uint16_t stall_startup_ms;                     /* 卡死判断起步屏蔽。 */
    uint16_t stall_timeout_ms;                     /* 编码器停滞持续时间。 */
    uint16_t stall_min_delta_position;             /* 编码器最小有效变化。 */
    uint16_t stall_target_margin_position;         /* 到位编码误差。 */
} FeetechStsManagerConfig_t;

typedef struct {
    uint8_t id;                                    /* 当前反馈来源 ID。 */
    uint8_t online;                                /* 1：最近收到有效舵机应答。 */
    uint8_t error;                                 /* 最近应答 ERROR 字段。 */
    uint32_t last_online_tick;                     /* 最近在线更新时间。 */
} FeetechStsManagerDevice_t;

typedef struct {
    uint16_t position;                             /* 当前位置原始值。 */
    float angle_deg;                               /* 当前角度，单位：度。 */
    int16_t speed;                                 /* 当前速度原始值。 */
    int16_t load;                                  /* 当前负载原始值。 */
    uint16_t load_abs;                             /* 当前负载绝对值。 */
    float angle_filtered_deg;                      /* 当前角度滤波值，减少无人机振动导致的显示和判断抖动。 */
    uint16_t load_abs_filtered;                    /* 当前负载绝对值滤波值。 */

    uint8_t voltage_raw;                           /* 当前电压原始值，通常 0.1V/单位。 */
    uint16_t voltage_mv;                           /* 当前电压，单位：mV。 */
    uint8_t temperature_c;                         /* 当前温度，单位：摄氏度。 */
    uint8_t moving;                                /* 当前运动标志，1 运动，0 停止。 */
    int16_t current_raw;                           /* 当前电流原始值。 */
    uint16_t current_ma;                           /* 当前电流，单位：mA。 */
    uint16_t current_ma_filtered;                  /* 当前电流滤波值，单位：mA。 */

    uint8_t position_valid;                        /* 位置/角度是否有效。 */
    uint8_t telemetry_valid;                       /* 电压/温度/电流/负载是否有效。 */
    uint8_t update_flags;                          /* 最近反馈更新标志。 */
    uint32_t update_count;                         /* 有效应答包计数。 */
    uint32_t update_tick;                          /* 最近反馈更新时间。 */
} FeetechStsManagerFeedback_t;

typedef struct {
    float target_angle_deg;                        /* 当前目标角度，单位：度。 */
    uint16_t target_position;                      /* 当前目标位置。 */
    uint16_t target_time_ms;                       /* 当前目标动作时间。 */
    uint16_t target_speed;                         /* 当前目标速度。 */
    uint8_t target_acc;                            /* 当前目标加速度。 */

    uint16_t torque_limit;                         /* 当前目标转矩限制，0~1000。 */
    uint8_t torque_enable;                         /* 当前期望扭矩开关，1 上力，0 卸力。 */
    uint8_t mode;                                  /* 当前期望运行模式。 */
    uint8_t reached;                               /* 1：反馈角度已接近目标角度。 */
    uint32_t update_tick;                          /* 最近目标更新时间。 */
} FeetechStsManagerTarget_t;

typedef struct {
    FeetechStsControlState_t state;                /* 当前状态机状态。 */
    FeetechStsGripProfile_t profile;               /* 当前夹取档位。 */
    FeetechStsGripResult_t result;                 /* 最近夹取结果。 */
    uint8_t object_detected;                       /* 1：已检测到物体。 */

    uint8_t sweep_active;                          /* 1：B0 扫描运行中。 */
    uint8_t sweep_direction_close;                 /* 1：扫描方向为闭合；0：扫描方向为张开。 */
    uint8_t last_motion_direction_close;             /* 最近有效运动方向：1 闭合，0 张开。 */
    uint8_t grip_trigger_counter;                  /* 连续触发计数。 */
    uint8_t overload_active;                       /* 1：过载保护计时中。 */
    uint8_t low_voltage_active;                    /* 1：当前处于低电压保护。 */
    uint8_t release_permission;                    /* 1：允许释放。 */
    uint8_t release_armed;                         /* 1：释放已授权。 */
    uint8_t slip_compensate_count;                 /* 防滑补夹次数。 */
    uint8_t limit_setting_active;                  /* 1：机械极限标定中。 */
    uint8_t limits_valid;                          /* 1：机械极限有效。 */
    FeetechStsSweepRange_t sweep_range;            /* A0/B0 公共动作范围。 */

    uint32_t state_tick_ms;                        /* 当前状态进入时间。 */
    uint32_t last_feedback_tick_ms;                /* 最近主动请求反馈时间。 */
} FeetechStsManagerAction_t;

typedef struct {
    uint8_t last_instruction;                      /* 最近一次发送指令码。 */
    uint8_t last_address;                          /* 最近一次读写地址。 */
    uint8_t last_data_len;                         /* 最近一次读写长度。 */
    uint8_t last_error;                            /* 最近一次 ERROR 字段。 */
    HAL_StatusTypeDef last_hal_status;             /* 最近一次 HAL 状态。 */

    uint32_t tx_count;                             /* 底层发送计数。 */
    uint32_t rx_count;                             /* 底层有效接收计数。 */
    uint32_t parse_error_count;                    /* 协议解析错误计数。 */
    uint32_t last_tx_tick;                         /* 最近发送时间。 */
    uint32_t last_rx_tick;                         /* 最近接收时间。 */
} FeetechStsManagerComm_t;

typedef struct {
    FeetechStsControlOwner_t owner;                /* 当前写控制权来源。 */
    FeetechStsControlMode_t mode;                  /* 当前控制模式。 */
    FeetechStsFaultLevel_t fault_level;            /* 当前故障等级。 */
    uint8_t local_write_enable;                    /* 1：最新命令来自按键，允许本地状态机续写目标。 */
    uint32_t last_external_write_tick_ms;          /* 最近一次外部写帧时间。 */
    uint32_t owner_timeout_ms;                     /* 控制权超时时间。 */
} FeetechStsManagerAuthority_t;

typedef struct {
    uint8_t print_enable;                          /* 1：允许自动打印状态；0：只更新结构体不打印。 */
    uint16_t print_period_ms;                      /* 自动打印周期，单位：ms。 */
    uint32_t last_print_tick;                      /* 最近一次自动打印时间。 */
    uint8_t screen_update_request;                 /* 预留：屏幕强制刷新请求。 */
} FeetechStsManagerOption_t;

typedef struct {
    FeetechStsManagerConfig_t config;              /* 运行基准默认值和可调配置。 */

    FeetechStsManagerDevice_t device;              /* 舵机身份、在线和错误状态。 */
    FeetechStsManagerFeedback_t feedback;          /* 舵机回读后的物理反馈。 */
    FeetechStsManagerTarget_t target;              /* 当前目标和期望输出。 */
    FeetechStsManagerAction_t action;              /* 本地动作状态机信息。 */
    FeetechStsManagerComm_t comm;                  /* 通信统计和最近一次指令信息。 */
    FeetechStsManagerAuthority_t authority;        /* 当前控制权、模式和故障等级。 */
    FeetechStsManagerOption_t option;              /* 打印、屏幕刷新等运行选项。 */
} FeetechStsManager_t;


/*============================================================
 * 可调参数表
 *============================================================*/

typedef enum {
    FEETECH_STS_PARAM_OPEN_ANGLE = 0,              /* 张开角度，单位：度。 */
    FEETECH_STS_PARAM_CLOSE_ANGLE,                 /* 闭合角度，单位：度。 */
    FEETECH_STS_PARAM_PROFILE,                     /* 夹取档位，0 软，1 中，2 硬，3 易碎。 */
    FEETECH_STS_PARAM_DEFAULT_SPEED,               /* 普通默认速度。 */
    FEETECH_STS_PARAM_OPEN_SPEED,                  /* 张开速度。 */
    FEETECH_STS_PARAM_CLOSE_SPEED,                 /* 闭合/夹取速度。 */
    FEETECH_STS_PARAM_SWEEP_SPEED,                 /* 扫描速度。 */
    FEETECH_STS_PARAM_SWEEP_STEP,                  /* 扫描步进角度，单位：度。 */
    FEETECH_STS_PARAM_SWEEP_STEP_MS,               /* 扫描步进周期，单位：ms。 */
    FEETECH_STS_PARAM_FULL_TORQUE,                 /* 运动阶段转矩限制。 */
    FEETECH_STS_PARAM_HOLD_TORQUE,                 /* 保持阶段转矩限制。 */
    FEETECH_STS_PARAM_FEEDBACK_MS,                 /* 普通反馈周期，单位：ms。 */
    FEETECH_STS_PARAM_IDLE_FEEDBACK_MS,            /* 空闲反馈周期，单位：ms。 */
    FEETECH_STS_PARAM_MIN_VALID_MOVE,              /* 旧参数兼容，A0 新逻辑不使用。 */
    FEETECH_STS_PARAM_GRIP_SETTLE_MS,              /* 夹取稳定确认时间，单位：ms。 */
    FEETECH_STS_PARAM_LOW_VOLTAGE_MV,              /* 低电压保护阈值，单位：mV。 */
    FEETECH_STS_PARAM_SLIP_ANGLE,                  /* 防滑角度阈值，单位：度。 */
    FEETECH_STS_PARAM_SLIP_COMPENSATE,             /* 防滑补夹角度，单位：度。 */
    FEETECH_STS_PARAM_RELEASE_PERMISSION,          /* 释放允许开关。 */
    FEETECH_STS_PARAM_PRINT_ENABLE,                /* 自动打印开关。 */
    FEETECH_STS_PARAM_PRINT_PERIOD_MS,             /* 自动打印周期，单位：ms。 */
    FEETECH_STS_PARAM_COUNT
} FeetechStsParamId_t;

typedef struct {
    FeetechStsParamId_t id;                        /* 参数编号。 */
    const char *name;                              /* 参数英文名，运行时字符串保持 ASCII，避免 IAR 多字节警告。 */
    int32_t value;                                 /* 当前值，角度类按“度”整数保存。 */
    int32_t default_value;                         /* 默认值。 */
    int32_t min_value;                             /* 最小允许值。 */
    int32_t max_value;                             /* 最大允许值。 */
    int32_t step;                                  /* 建议调节步长。 */
    uint8_t writable;                              /* 1：允许通过上位机调节。 */
} FeetechStsParamInfo_t;

/*============================================================
 * 3. 对外接口
 *============================================================*/

void FeetechStsControl_Init(void);
void FeetechStsControl_Task(void);

HAL_StatusTypeDef FeetechStsControl_ConfigureServo(void);

HAL_StatusTypeDef FeetechStsControl_MoveToAngle(float angle_deg, uint16_t time_ms);
HAL_StatusTypeDef FeetechStsControl_Open(void);
HAL_StatusTypeDef FeetechStsControl_Release(void);
HAL_StatusTypeDef FeetechStsControl_Close(void);
HAL_StatusTypeDef FeetechStsControl_StartGrip(void);
HAL_StatusTypeDef FeetechStsControl_ToggleOpenClose(void);
HAL_StatusTypeDef FeetechStsControl_StartSweep(void);
HAL_StatusTypeDef FeetechStsControl_StopSweep(void);
HAL_StatusTypeDef FeetechStsControl_ArmRelease(void);
HAL_StatusTypeDef FeetechStsControl_ConfirmRelease(void);
HAL_StatusTypeDef FeetechStsControl_SetCarryHold(void);
HAL_StatusTypeDef FeetechStsControl_HoldCurrent(void);
HAL_StatusTypeDef FeetechStsControl_StopMotion(void);
HAL_StatusTypeDef FeetechStsControl_RequestFeedback(void);
HAL_StatusTypeDef FeetechStsControl_BeginLimitSetting(void);
HAL_StatusTypeDef FeetechStsControl_CancelLimitSetting(void);
HAL_StatusTypeDef FeetechStsControl_FinishLimitSetting(void);
HAL_StatusTypeDef FeetechStsControl_RecordCloseLimit(void);
HAL_StatusTypeDef FeetechStsControl_RecordOpenLimit(void);
HAL_StatusTypeDef FeetechStsControl_ClearStoredLimits(void);
uint8_t FeetechStsControl_AreLimitsValid(void);
uint8_t FeetechStsControl_IsLimitSetting(void);
HAL_StatusTypeDef FeetechStsControl_SetSweepRange(FeetechStsSweepRange_t range);
HAL_StatusTypeDef FeetechStsControl_SaveSweepRange(FeetechStsSweepRange_t range);
FeetechStsSweepRange_t FeetechStsControl_GetSweepRange(void);
uint8_t FeetechStsControl_IsMotionRangeReady(void); // 当前共同动作范围是否可用。
HAL_StatusTypeDef FeetechStsControl_ReverseSweep(void);
uint8_t FeetechStsControl_IsMotionState(void);

const FeetechStsControlStatus_t *FeetechStsControl_GetStatus(void);
FeetechStsControlState_t FeetechStsControl_GetState(void);
float FeetechStsControl_GetTargetAngle(void);
float FeetechStsControl_GetActualAngle(void);
uint16_t FeetechStsControl_GetVoltageMv(void);
uint8_t FeetechStsControl_GetTemperatureC(void);
uint16_t FeetechStsControl_GetLoadAbs(void);
uint16_t FeetechStsControl_GetCurrentMa(void);
const char *FeetechStsControl_GetStateText(void);
const char *FeetechStsControl_GetResultText(void);
const char *FeetechStsControl_GetOwnerText(void);
const char *FeetechStsControl_GetModeText(void);
const char *FeetechStsControl_GetFaultReasonText(void);
uint8_t FeetechStsControl_TakeSweepPrintEvent(FeetechStsSweepPrintEvent_t *out_event);
uint8_t FeetechStsControl_TakeMotionPrintEvent(FeetechStsMotionPrintEvent_t *out_event);
FeetechStsControlOwner_t FeetechStsControl_GetOwner(void);
FeetechStsControlMode_t FeetechStsControl_GetMode(void);
uint8_t FeetechStsControl_IsLocalWriteEnabled(void);
HAL_StatusTypeDef FeetechStsControl_ExternalWriteBegin(FeetechStsControlOwner_t owner);
HAL_StatusTypeDef FeetechStsControl_ApplyExternalTarget(float angle_deg, uint16_t time_ms, uint16_t speed, uint8_t acc, FeetechStsControlOwner_t owner);
HAL_StatusTypeDef FeetechStsControl_CommandAngle(float angle_deg, FeetechStsControlOwner_t owner); // 上位机角度 -> 机械极限限幅 -> STS/PWM。
void FeetechStsControl_SetActionSource(FeetechStsControlOwner_t owner); // 更新状态机动作来源。
float FeetechStsControl_GetMotionMinAngle(void); // 读取当前安全最小角度。
float FeetechStsControl_GetMotionMaxAngle(void); // 读取当前安全最大角度。
HAL_StatusTypeDef FeetechStsControl_SyncActualAsTarget(void);

const FeetechStsManager_t *FeetechStsControl_GetManager(void);
void FeetechStsControl_SyncManager(void);
void FeetechStsControl_SetPrintEnable(uint8_t enable);
uint8_t FeetechStsControl_GetPrintEnable(void);
void FeetechStsControl_SetReleasePermission(uint8_t enable);
uint8_t FeetechStsControl_GetReleasePermission(void);

uint8_t FeetechStsControl_GetParamCount(void);
uint8_t FeetechStsControl_GetParamInfo(FeetechStsParamId_t id, FeetechStsParamInfo_t *out_info);
uint8_t FeetechStsControl_GetParamValue(FeetechStsParamId_t id, int32_t *out_value);
uint8_t FeetechStsControl_SetParamValue(FeetechStsParamId_t id, int32_t value);
uint8_t FeetechStsControl_ResetParamValue(FeetechStsParamId_t id);
void FeetechStsControl_ResetAllParams(void);
const char *FeetechStsControl_GetParamName(FeetechStsParamId_t id);


/* 自适应兼容接口：默认关闭，集中放置便于后续移除。 */
HAL_StatusTypeDef FeetechStsControl_SetProfile(FeetechStsGripProfile_t profile);
const FeetechStsGripConfig_t *FeetechStsControl_GetProfileConfig(FeetechStsGripProfile_t profile);
FeetechStsGripResult_t FeetechStsControl_GetGripResult(void);
uint8_t FeetechStsControl_IsObjectDetected(void);
uint8_t FeetechStsControl_TakeGripDetectEvent(FeetechStsGripDetectEvent_t *out_event);

#ifdef __cplusplus
}
#endif

#endif
