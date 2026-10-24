/*********************************************************************************************************************
 * @file    feature_config.h
 * @brief   夹爪编译开关、默认模式和容量配置。
 *********************************************************************************************************************/

#ifndef __FEATURE_CONFIG_H__
#define __FEATURE_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef APP_HOST_DIAGNOSTICS_ENABLE
#define APP_HOST_DIAGNOSTICS_ENABLE 0U /* P3: HOST 由 VOFA 协议占用；P2 回显模式关闭 */
#endif

/* P3 只读识别/反馈：先不初始化命令层和机械动作。 */
#ifndef APP_P3_SERVO_ENABLE
#define APP_P3_SERVO_ENABLE 1U
#endif
#ifndef APP_P3_MOTION_ENABLE
#define APP_P3_MOTION_ENABLE 1U /* P3a 只读成功后接入原有 command/control；按键/帧命令才动作 */
#endif
#ifndef APP_STARTUP_LOG_ENABLE
#define APP_STARTUP_LOG_ENABLE 0U
#endif
#ifndef APP_P3_TELEMETRY_ENABLE
#define APP_P3_TELEMETRY_ENABLE 0U
#endif
#ifndef APP_P3_TELEMETRY_PERIOD_MS
#define APP_P3_TELEMETRY_PERIOD_MS 500U
#endif

// ======================================================
// 1. 编译期开关
// ======================================================

#define CLAW_FEATURE_RX_FRAME_ECHO_ENABLE          0U   // 1=回显收到的官方帧。
#define CLAW_FEATURE_VOFA_TELEMETRY_SUPPORT        1U   // 1=编译 claw 周期/单次反馈。
#define CLAW_FEATURE_BLUETOOTH_SLIDER_ENABLE       1U   // 1=接收蓝牙整数滑块包。
#define CLAW_FEATURE_STS_REGISTER_TABLE_ENABLE      1U   // 1=启用官方寄存器名称和单位。
#define CLAW_FEATURE_STS_REGISTER_GUARD_ENABLE      1U   // 1=检查官方写入权限和范围。
#define CLAW_FEATURE_STS_DANGEROUS_WRITE_ENABLE     0U   // 1=全局开放危险寄存器。
#define CLAW_FEATURE_STS_UNKNOWN_WRITE_ENABLE       0U   // 1=允许未知地址原样透传。
#define CLAW_FEATURE_STS_WRITE_ACK_WAIT_ENABLE      0U   // 1=非广播写等待官方 ACK。
#define CLAW_FEATURE_STS_WRITE_READBACK_ENABLE      0U   // 预留：本次不自动读回。
#define CLAW_FEATURE_STS_MAX_VOLTAGE_WRITE_ENABLE   1U   // 1=单独允许写最高输入电压。
#define CLAW_FEATURE_STS_MAX_VOLTAGE_RAW           74U   // C001 当前规格上限7.4V；不得沿用其他后缀的11.7V。

#define CLAW_FEATURE_ADAPTIVE_GRIP_ENABLE          0U   // 1=启用负载/电流夹取识别。
#define CLAW_FEATURE_SLIP_PROTECT_ENABLE           0U   // 1=启用保持态防滑补夹。
#define CLAW_FEATURE_STALL_STOP_ENABLE             1U   // 1=启用编码器停滞停止。

// ======================================================
// 2. 默认运行模式
// ======================================================

#define CLAW_DEFAULT_AUTO_PRINT_ENABLE             0U   // 上电默认关闭 claw 周期打印。
#define CLAW_DEFAULT_AUTO_PRINT_PERIOD_MS         100U  // claw 周期打印间隔。
#define CLAW_DEFAULT_MOTION_RANGE                  1U   // 0=机械极限；1=0~270度。

#define CLAW_DEFAULT_BLUETOOTH_SLIDER_HEAD       0xA5U  // 滑块包头。
#define CLAW_DEFAULT_BLUETOOTH_SLIDER_TAIL       0x5AU  // 滑块包尾。
#define CLAW_DEFAULT_BLUETOOTH_SLIDER_FORMAT        0U  // 0=ASCII；1=U8；2=U16LE；3=U32LE。
#define CLAW_DEFAULT_BLUETOOTH_ANGLE_CHANNEL        0U  // 通道 0 -> 角度滑块。


#define CLAW_DEFAULT_RANGE_SELECT_HOLD_MS        3000U  // A 单独长按进入行程选择。
#define CLAW_DEFAULT_LIMIT_COMBO_HOLD_MS         3000U  // A+B 长按进入/保存标定。
#define CLAW_DEFAULT_CALIBRATION_SWEEP_SPEED       200U  // 机械极限标定扫描速度。
#define CLAW_DEFAULT_STALL_STARTUP_MS              300U  // 新运动起步屏蔽。
#define CLAW_DEFAULT_STALL_TIMEOUT_MS              800U  // 编码持续不变判定时间。
#define CLAW_DEFAULT_STALL_MIN_DELTA_POSITION        2U  // 编码器最小有效变化。
#define CLAW_DEFAULT_STALL_TARGET_MARGIN_POSITION   35U  // 到位误差，约 3 度。

// ======================================================
// 3. 容量配置
// ======================================================

#define CLAW_STATUS_TEXT_CAPACITY                 384U  // 单条文本最大长度。
#define CLAW_BLUETOOTH_RX_FRAME_QUEUE_DEPTH         4U  // 蓝牙完整帧队列深度。
#define CLAW_BLUETOOTH_TX_BUFFER_SIZE             1024U // 蓝牙非阻塞发送队列容量。
#define CLAW_BLUETOOTH_TX_CHUNK_MAX_LEN             64U // 单次中断发送块长度。
#define CLAW_BLUETOOTH_RX_PACKET_TIMEOUT_MS        100U // 未完成帧复位时间。

// ======================================================
// 4. 默认值快照
// ======================================================

typedef struct {
    uint8_t auto_print_enable;                    // claw 周期打印默认开关。
    uint16_t auto_print_period_ms;                // claw 周期打印默认间隔。
    uint8_t motion_range;                         // A0/B0/滑块共同动作范围。
    uint16_t range_select_hold_ms;                // A 单独长按阈值。
    uint16_t limit_combo_hold_ms;                 // A+B 组合长按阈值。
    uint16_t calibration_sweep_speed;              // 机械极限标定扫描速度。
    uint16_t stall_startup_ms;                    // 卡死判断起步屏蔽。
    uint16_t stall_timeout_ms;                    // 卡死持续时间。
    uint16_t stall_min_delta_position;            // 编码器最小有效变化。
    uint16_t stall_target_margin_position;        // 到位编码误差。
} ClawFeatureDefaults_t;

extern const ClawFeatureDefaults_t g_claw_feature_defaults;

#ifdef __cplusplus
}
#endif

#endif
