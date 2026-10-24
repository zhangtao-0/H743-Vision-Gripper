/* Legacy API bridge: original TIM3 logical channels now use H743 TIM4 on
 * PD12/PD13. Never reconfigure PA6 (camera) or PA0 (button) as PWM. */
#include "tim.h"
#include "board_peripherals.h"
#include "board_config.h"
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3; /* retained source ABI; no physical TIM3 output owned */
void MX_TIM2_Init(void) {
 __HAL_RCC_TIM2_CLK_ENABLE();
 htim2.Instance=TIM2;htim2.Init.Prescaler=BOARD_PWM_KERNEL_HZ/1000000U-1U;
 htim2.Init.CounterMode=TIM_COUNTERMODE_UP;htim2.Init.Period=65535U;
 htim2.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
 (void)HAL_TIM_Base_Init(&htim2);
}
void MX_TIM3_Init(void) { /* PWM channels are initialized lazily on explicit output. */ }
HAL_StatusTypeDef TIM3_SERVO_PWM_SetPulseUs(Tim3ServoPwmChannel_t channel,uint16_t pulse_us) {
 return BoardPwm_SetChannelUs((uint8_t)channel,pulse_us);
}
HAL_StatusTypeDef TIM3_SERVO_PWM_StartChannel(Tim3ServoPwmChannel_t channel) {
 return (unsigned)channel<TIM3_SERVO_PWM_CHANNEL_COUNT?HAL_OK:HAL_ERROR;
}
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timer) { (void)timer; }
