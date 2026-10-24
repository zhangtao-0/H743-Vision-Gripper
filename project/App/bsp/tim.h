/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.h
  * @brief   This file contains all the function prototypes for
  *          the tim.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

#define TIM2_CH1_PWM_PIN_ENABLE 0U      /* PA0 已用于 A0 按键，TIM2_CH1 保持关闭。 */
#define TIM3_CH1_PWM_PIN_ENABLE 1U      /* 兼容通道1：PD12 / TIM4_CH1。 */
#define TIM3_CH4_PWM_PIN_ENABLE 1U      /* 兼容通道2：PD13 / TIM4_CH2。 */
#define TIM3_SERVO_PWM_PERIOD_US 20000U /* PWM 舵机周期：20ms。 */

typedef enum {
    TIM3_SERVO_PWM_CHANNEL_1 = 0,  /* PD12 / TIM4_CH1。 */
    TIM3_SERVO_PWM_CHANNEL_2,      /* PD13 / TIM4_CH2。 */
    TIM3_SERVO_PWM_CHANNEL_COUNT
} Tim3ServoPwmChannel_t;

void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
HAL_StatusTypeDef TIM3_SERVO_PWM_StartChannel(Tim3ServoPwmChannel_t channel);
HAL_StatusTypeDef TIM3_SERVO_PWM_SetPulseUs(Tim3ServoPwmChannel_t channel, uint16_t pulse_us);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#ifdef __cplusplus
}
#endif

#endif