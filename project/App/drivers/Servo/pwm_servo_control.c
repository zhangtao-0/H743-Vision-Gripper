/*********************************************************************************************************************
 * @file    pwm_servo_control.c
 * @brief   两路独立 PWM 舵机控制实现。
 *********************************************************************************************************************/

#include "pwm_servo_control.h"
#include "tim.h"
#include "integration_config.h"
#include "bsp_key.h"
#include <string.h>

/* 两只舵机的固定硬件和动作配置相互独立。 */
static const PwmServoConfig_t s_pwm_servo_config[PWM_SERVO_ID_COUNT] = {
    {
        TIM3_SERVO_PWM_CHANNEL_1,
        PWM_SERVO_1_MIN_PULSE_US,
        PWM_SERVO_1_MAX_PULSE_US,
        PWM_SERVO_1_MIN_ANGLE_DEG,
        PWM_SERVO_1_MAX_ANGLE_DEG,
        PWM_SERVO_1_OPEN_ANGLE_DEG,
        PWM_SERVO_1_CLOSE_ANGLE_DEG,
        PWM_SERVO_1_REVERSE
    },
    {
        TIM3_SERVO_PWM_CHANNEL_2,
        PWM_SERVO_2_MIN_PULSE_US,
        PWM_SERVO_2_MAX_PULSE_US,
        PWM_SERVO_2_MIN_ANGLE_DEG,
        PWM_SERVO_2_MAX_ANGLE_DEG,
        PWM_SERVO_2_OPEN_ANGLE_DEG,
        PWM_SERVO_2_CLOSE_ANGLE_DEG,
        PWM_SERVO_2_REVERSE
    }
};

static PwmServoManager_t s_pwm_servo_manager;

static uint8_t PwmServoControl_IsValidId(PwmServoId_t id);
static float PwmServoControl_LimitFloat(float value, float min_value, float max_value);
static uint16_t PwmServoControl_AngleToPulse(
    const PwmServoConfig_t *config,
    float angle_deg);
static HAL_StatusTypeDef PwmServoControl_Output(
    PwmServoId_t id,
    float angle_deg,
    PwmServoPosition_t position);
static uint8_t PwmServoControl_GetKeyPressed(PwmServoId_t id);

/* 初始化运行状态；上电不启动 PWM，也不主动转动舵机。 */
void PwmServoControl_Init(void)
{
    uint8_t index;

    memset(&s_pwm_servo_manager, 0, sizeof(s_pwm_servo_manager));

    for (index = 0U; index < (uint8_t)PWM_SERVO_ID_COUNT; index++) {
        s_pwm_servo_manager.servo[index].position =
            PWM_SERVO_POSITION_UNKNOWN;
        s_pwm_servo_manager.servo[index].target_angle_deg =
            s_pwm_servo_config[index].open_angle_deg;
        s_pwm_servo_manager.servo[index].last_key_pressed =
            PwmServoControl_GetKeyPressed((PwmServoId_t)index);
        s_pwm_servo_manager.servo[index].last_status = HAL_OK;
    }
}

/* 两个独立按键均使用“按下一次切换一次”；首次按下先执行张开。 */
void PwmServoControl_Task(void)
{
    uint8_t index;
    uint8_t pressed;
    uint8_t virtual_pressed;
    PwmServoRuntime_t *runtime;

    for (index = 0U; index < (uint8_t)PWM_SERVO_ID_COUNT; index++) {
        runtime = &s_pwm_servo_manager.servo[index];
        pressed = PwmServoControl_GetKeyPressed((PwmServoId_t)index);
        virtual_pressed = runtime->virtual_key_pressed;

        if (((pressed != 0U) &&
             (runtime->last_key_pressed == 0U)) ||
            ((virtual_pressed != 0U) &&
             (runtime->last_virtual_pressed == 0U))) {
            (void)PwmServoControl_Toggle((PwmServoId_t)index);
        }

        runtime->last_key_pressed = pressed;
        runtime->last_virtual_pressed = virtual_pressed;
    }
}

/* 控制指定舵机到张开位置。 */
HAL_StatusTypeDef PwmServoControl_Open(PwmServoId_t id)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    return PwmServoControl_Output(
        id,
        s_pwm_servo_config[id].open_angle_deg,
        PWM_SERVO_POSITION_OPEN);
}

/* 控制指定舵机到闭合位置。 */
HAL_StatusTypeDef PwmServoControl_Close(PwmServoId_t id)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    return PwmServoControl_Output(
        id,
        s_pwm_servo_config[id].close_angle_deg,
        PWM_SERVO_POSITION_CLOSE);
}

/* 独立切换开合；未知位置时第一次先张开，避免直接夹紧。 */
HAL_StatusTypeDef PwmServoControl_Toggle(PwmServoId_t id)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    if (s_pwm_servo_manager.servo[id].position ==
        PWM_SERVO_POSITION_OPEN) {
        return PwmServoControl_Close(id);
    }

    return PwmServoControl_Open(id);
}

/* 设置指定舵机任意逻辑角度。 */
HAL_StatusTypeDef PwmServoControl_SetAngle(
    PwmServoId_t id,
    float angle_deg)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    return PwmServoControl_Output(
        id,
        angle_deg,
        PWM_SERVO_POSITION_CUSTOM);
}

/* 设置上位机虚拟按键电平。 */
HAL_StatusTypeDef PwmServoControl_SetVirtualKeyLevel(
    PwmServoId_t id,
    uint8_t pressed)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    s_pwm_servo_manager.servo[id].virtual_key_pressed =
        (pressed != 0U) ? 1U : 0U;
    return HAL_OK;
}

/* 读取指定舵机配置。 */
const PwmServoConfig_t *PwmServoControl_GetConfig(PwmServoId_t id)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return 0;
    }

    return &s_pwm_servo_config[id];
}

/* 读取指定舵机运行状态。 */
const PwmServoRuntime_t *PwmServoControl_GetRuntime(PwmServoId_t id)
{
    if (PwmServoControl_IsValidId(id) == 0U) {
        return 0;
    }

    return &s_pwm_servo_manager.servo[id];
}

/* 读取两路 PWM 舵机管理器。 */
const PwmServoManager_t *PwmServoControl_GetManager(void)
{
    return &s_pwm_servo_manager;
}

static uint8_t PwmServoControl_IsValidId(PwmServoId_t id)
{
    return ((uint8_t)id < (uint8_t)PWM_SERVO_ID_COUNT) ?
           1U : 0U;
}

static float PwmServoControl_LimitFloat(
    float value,
    float min_value,
    float max_value)
{
    float temp;

    if (min_value > max_value) {
        temp = min_value;
        min_value = max_value;
        max_value = temp;
    }

    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

/* 逻辑角度 -> 独立反向 -> 独立脉宽。 */
static uint16_t PwmServoControl_AngleToPulse(
    const PwmServoConfig_t *config,
    float angle_deg)
{
    float limited_angle;
    float physical_angle;
    float angle_range;
    float pulse_range;
    float ratio;
    float pulse;

    if (config == 0) {
        return 0U;
    }

    limited_angle = PwmServoControl_LimitFloat(
        angle_deg,
        config->min_angle_deg,
        config->max_angle_deg);

    if (config->reverse != 0U) {
        physical_angle =
            config->max_angle_deg +
            config->min_angle_deg -
            limited_angle;
    } else {
        physical_angle = limited_angle;
    }

    angle_range =
        config->max_angle_deg -
        config->min_angle_deg;

    if (angle_range == 0.0f) {
        ratio = 0.0f;
    } else {
        ratio =
            (physical_angle -
             config->min_angle_deg) /
            angle_range;
    }

    ratio = PwmServoControl_LimitFloat(
        ratio,
        0.0f,
        1.0f);

    pulse_range =
        (float)config->max_pulse_us -
        (float)config->min_pulse_us;

    pulse =
        (float)config->min_pulse_us +
        pulse_range * ratio;

    if (pulse < 0.0f) {
        return 0U;
    }

    if (pulse > 65535.0f) {
        return 65535U;
    }

    return (uint16_t)(pulse + 0.5f);
}

static HAL_StatusTypeDef PwmServoControl_Output(
    PwmServoId_t id,
    float angle_deg,
    PwmServoPosition_t position)
{
    const PwmServoConfig_t *config;
    PwmServoRuntime_t *runtime;
    HAL_StatusTypeDef status;
    float limited_angle;
    uint16_t pulse_us;

    if (PwmServoControl_IsValidId(id) == 0U) {
        return HAL_ERROR;
    }

    config = &s_pwm_servo_config[id];
    runtime = &s_pwm_servo_manager.servo[id];

    limited_angle = PwmServoControl_LimitFloat(
        angle_deg,
        config->min_angle_deg,
        config->max_angle_deg);

    pulse_us =
        PwmServoControl_AngleToPulse(
            config,
            limited_angle);

    status = TIM3_SERVO_PWM_SetPulseUs(
        (Tim3ServoPwmChannel_t)config->timer_channel_id,
        pulse_us);
    if (status != HAL_OK) {
        runtime->last_status = status;
        return status;
    }

    if (runtime->started == 0U) {
        status = TIM3_SERVO_PWM_StartChannel(
            (Tim3ServoPwmChannel_t)config->timer_channel_id);
        if (status != HAL_OK) {
            runtime->last_status = status;
            return status;
        }
        runtime->started = 1U;
    }

    runtime->last_status = status;

    if (status == HAL_OK) {
        runtime->target_angle_deg = limited_angle;
        runtime->pulse_us = pulse_us;
        runtime->position = position;
    }

    return status;
}

/* 独立按键映射：Key_Task3 控制舵机1，Key_Task4 控制舵机2。 */
static uint8_t PwmServoControl_GetKeyPressed(PwmServoId_t id)
{
#if !BOARD_KEYS34_CONTROL_PWM
    (void)id; return 0U;
#else
    if (id == PWM_SERVO_ID_1) {
        return Key_GetFlag(Key_Task3);
    }

    if (id == PWM_SERVO_ID_2) {
        return Key_GetFlag(Key_Task4);
    }

    return 0U;
#endif
}
