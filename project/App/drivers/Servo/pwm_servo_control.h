/*********************************************************************************************************************
 * @file    pwm_servo_control.h
 * @brief   两路独立 PWM 舵机控制接口。
 *
 * @details
 * 1. PWM 舵机 1：PD12 / TIM4_CH1，由独立按键 1 控制开合。
 * 2. PWM 舵机 2：PD13 / TIM4_CH2，由独立按键 2 控制开合。
 * 3. 本模块不依赖飞特 STS 舵机、VOFA、蓝牙或机械夹爪状态机。
 * 4. 两只舵机分别配置脉宽、角度、开合位置和反向。
 *********************************************************************************************************************/

#ifndef __PWM_SERVO_CONTROL_H__
#define __PWM_SERVO_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_hal.h"
#include <stdint.h>

/* 0~180度映射500~2500us；按键动作限制在0~90度。 */
#define PWM_SERVO_1_MIN_PULSE_US           500U
#define PWM_SERVO_1_MAX_PULSE_US          2500U
#define PWM_SERVO_1_MIN_ANGLE_DEG         0.0f
#define PWM_SERVO_1_MAX_ANGLE_DEG         180.0f
#define PWM_SERVO_1_OPEN_ANGLE_DEG        0.0f
#define PWM_SERVO_1_CLOSE_ANGLE_DEG        90.0f
#define PWM_SERVO_1_REVERSE               0U

#define PWM_SERVO_2_MIN_PULSE_US           500U
#define PWM_SERVO_2_MAX_PULSE_US          2500U
#define PWM_SERVO_2_MIN_ANGLE_DEG         0.0f
#define PWM_SERVO_2_MAX_ANGLE_DEG         180.0f
#define PWM_SERVO_2_OPEN_ANGLE_DEG        0.0f
#define PWM_SERVO_2_CLOSE_ANGLE_DEG        90.0f
#define PWM_SERVO_2_REVERSE               0U

typedef enum {
    PWM_SERVO_ID_1 = 0,
    PWM_SERVO_ID_2,
    PWM_SERVO_ID_COUNT
} PwmServoId_t;

typedef enum {
    PWM_SERVO_POSITION_UNKNOWN = 0,
    PWM_SERVO_POSITION_OPEN,
    PWM_SERVO_POSITION_CLOSE,
    PWM_SERVO_POSITION_CUSTOM
} PwmServoPosition_t;

typedef struct {
    uint8_t timer_channel_id;       /* TIM3 低层通道编号。 */
    uint16_t min_pulse_us;          /* 最小角对应脉宽。 */
    uint16_t max_pulse_us;          /* 最大角对应脉宽。 */
    float min_angle_deg;            /* 舵机最小逻辑角。 */
    float max_angle_deg;            /* 舵机最大逻辑角。 */
    float open_angle_deg;           /* 本机构张开角。 */
    float close_angle_deg;          /* 本机构闭合角。 */
    uint8_t reverse;                /* 1：角度映射反向。 */
} PwmServoConfig_t;

typedef struct {
    uint8_t started;                /* 1：对应 PWM 通道已经启动。 */
    uint8_t last_key_pressed;       /* 实体按键上一稳定电平。 */
    uint8_t virtual_key_pressed;     /* 上位机虚拟按键当前电平。 */
    uint8_t last_virtual_pressed;    /* 上位机虚拟按键上一电平。 */
    PwmServoPosition_t position;    /* 当前逻辑位置。 */
    float target_angle_deg;         /* 最近目标逻辑角。 */
    uint16_t pulse_us;              /* 最近输出脉宽。 */
    HAL_StatusTypeDef last_status;  /* 最近执行状态。 */
} PwmServoRuntime_t;

typedef struct {
    PwmServoRuntime_t servo[PWM_SERVO_ID_COUNT];
} PwmServoManager_t;

void PwmServoControl_Init(void);
void PwmServoControl_Task(void);

HAL_StatusTypeDef PwmServoControl_Open(PwmServoId_t id);
HAL_StatusTypeDef PwmServoControl_Close(PwmServoId_t id);
HAL_StatusTypeDef PwmServoControl_Toggle(PwmServoId_t id);
HAL_StatusTypeDef PwmServoControl_SetAngle(PwmServoId_t id, float angle_deg);
HAL_StatusTypeDef PwmServoControl_SetVirtualKeyLevel(PwmServoId_t id, uint8_t pressed);

const PwmServoConfig_t *PwmServoControl_GetConfig(PwmServoId_t id);
const PwmServoRuntime_t *PwmServoControl_GetRuntime(PwmServoId_t id);
const PwmServoManager_t *PwmServoControl_GetManager(void);

#ifdef __cplusplus
}
#endif

#endif
