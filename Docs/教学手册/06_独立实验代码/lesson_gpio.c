/* Independent lesson, not registered in the production EWP.
 * Preconditions: HAL_Init and the board SystemClock_Config have completed.
 * Do not initialize PG7 here while the production BoardLed owns it. */
#include "platform_hal.h"
static uint32_t s_last;
void Lesson_LedInit(void)
{
    GPIO_InitTypeDef io = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOG,GPIO_PIN_7,GPIO_PIN_SET);
    io.Pin=GPIO_PIN_7;io.Mode=GPIO_MODE_OUTPUT_PP;
    io.Pull=GPIO_NOPULL;io.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG,&io);s_last=HAL_GetTick();
}
void Lesson_LedTask(void)
{
    uint32_t now=HAL_GetTick();
    if((uint32_t)(now-s_last)>=500U){s_last=now;HAL_GPIO_TogglePin(GPIOG,GPIO_PIN_7);}
}
