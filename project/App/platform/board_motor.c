#include "board_gpio.h"
#include "board_motor.h"
#include "board_config.h"
#include "motor_config.h"
#include "motor_service.h"
static TIM_HandleTypeDef s_hall, s_pwm, s_tick;
static uint8_t s_ready;
uint8_t BoardMotor_DriverAvailable(void) {return MOTOR_DRIVER_PROFILE != 0;}
uint16_t BoardMotor_Count(void) {return s_hall.Instance ? (uint16_t)__HAL_TIM_GET_COUNTER(&s_hall) : 0U;}
uint8_t BoardMotor_Knob(void)
{return (HAL_GPIO_ReadPin(BOARD_MOTOR_KNOB_PORT,BOARD_MOTOR_KNOB_A)==GPIO_PIN_SET ? 2U : 0U) |
        (HAL_GPIO_ReadPin(BOARD_MOTOR_KNOB_PORT,BOARD_MOTOR_KNOB_B)==GPIO_PIN_SET ? 1U : 0U);}
uint8_t BoardMotor_Button(void)
{return HAL_GPIO_ReadPin(BOARD_MOTOR_KEY_PORT,BOARD_MOTOR_KEY_PIN)==BOARD_MOTOR_KEY_ACTIVE_STATE;}
void BoardMotor_Write(int16_t value)
{
#if MOTOR_DRIVER_PROFILE != 0
    int32_t v=(int32_t)value*MOTOR_OUTPUT_SIGN;
    uint32_t duty;
    if (!s_ready) return;
    if(v>1000)v=1000;if(v< -1000)v=-1000;
    duty=(uint32_t)(((uint64_t)(v<0 ? -v : v)*BOARD_MOTOR_PWM_PERIOD)/1000U);
    /* Timer runs at 20MHz, ARR=999 -> 20kHz. Output zero before changing GPIO direction. */
    __HAL_TIM_SET_COMPARE(&s_pwm,TIM_CHANNEL_1,0);
#if MOTOR_DRIVER_PROFILE == 3
    __HAL_TIM_SET_COMPARE(&s_pwm,TIM_CHANNEL_2,0);
    if(v>0)__HAL_TIM_SET_COMPARE(&s_pwm,TIM_CHANNEL_1,duty);
    if(v<0)__HAL_TIM_SET_COMPARE(&s_pwm,TIM_CHANNEL_2,duty);
#else
    HAL_GPIO_WritePin(BOARD_MOTOR_DIR_PORT,BOARD_MOTOR_IN1,v>0?GPIO_PIN_SET:GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BOARD_MOTOR_DIR_PORT,BOARD_MOTOR_IN2,v<0?GPIO_PIN_SET:GPIO_PIN_RESET);
#if MOTOR_DRIVER_PROFILE == 2
    HAL_GPIO_WritePin(BOARD_MOTOR_DIR_PORT,BOARD_MOTOR_STBY,v?GPIO_PIN_SET:GPIO_PIN_RESET);
#endif
    __HAL_TIM_SET_COMPARE(&s_pwm,TIM_CHANNEL_1,duty);
#endif
#else
    (void)value;
#endif
}
HAL_StatusTypeDef BoardMotor_Init(void)
{
    GPIO_InitTypeDef g={0};TIM_Encoder_InitTypeDef enc={0};TIM_OC_InitTypeDef oc={0};
    uint32_t apb1=HAL_RCC_GetPCLK1Freq(),apb2=HAL_RCC_GetPCLK2Freq();
    (void)oc;(void)s_pwm;
    /* Current board profile: HCLK=200MHz, both APBs /2, TIMPRE=0 -> timers=200MHz. */
    if(apb1!=BOARD_EXPECTED_PCLK1_HZ || apb2!=BOARD_EXPECTED_PCLK2_HZ || (RCC->CFGR&RCC_CFGR_TIMPRE))return HAL_ERROR;
    BoardGpio_EnableClock(BOARD_MOTOR_HALL_A_PORT);BoardGpio_EnableClock(BOARD_MOTOR_HALL_B_PORT);
    BoardGpio_EnableClock(BOARD_MOTOR_KNOB_PORT);BoardGpio_EnableClock(BOARD_MOTOR_KEY_PORT);
#if MOTOR_DRIVER_PROFILE != 0
    BoardGpio_EnableClock(BOARD_MOTOR_PWM_PORT);BoardGpio_EnableClock(BOARD_MOTOR_DIR_PORT);
#endif
    BOARD_MOTOR_HALL_CLOCK_ENABLE();BOARD_MOTOR_TICK_CLOCK_ENABLE();
    g.Mode=GPIO_MODE_AF_PP;g.Pull=BOARD_MOTOR_HALL_PULL;g.Speed=GPIO_SPEED_FREQ_LOW;
    g.Pin=BOARD_MOTOR_HALL_A_PIN;g.Alternate=BOARD_MOTOR_HALL_A_AF;HAL_GPIO_Init(BOARD_MOTOR_HALL_A_PORT,&g);
    g.Pin=BOARD_MOTOR_HALL_B_PIN;g.Alternate=BOARD_MOTOR_HALL_B_AF;HAL_GPIO_Init(BOARD_MOTOR_HALL_B_PORT,&g);
    s_hall.Instance=BOARD_MOTOR_HALL_TIM;s_hall.Init.Prescaler=0;s_hall.Init.CounterMode=TIM_COUNTERMODE_UP;
    s_hall.Init.Period=65535;s_hall.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    enc.EncoderMode=TIM_ENCODERMODE_TI12;enc.IC1Polarity=TIM_ICPOLARITY_RISING;enc.IC2Polarity=TIM_ICPOLARITY_RISING;
    enc.IC1Selection=enc.IC2Selection=TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler=enc.IC2Prescaler=TIM_ICPSC_DIV1;enc.IC1Filter=enc.IC2Filter=8;
    if(HAL_TIM_Encoder_Init(&s_hall,&enc)!=HAL_OK || HAL_TIM_Encoder_Start(&s_hall,TIM_CHANNEL_ALL)!=HAL_OK)return HAL_ERROR;
    g.Pin=BOARD_MOTOR_KNOB_A|BOARD_MOTOR_KNOB_B;g.Mode=GPIO_MODE_INPUT;g.Pull=BOARD_MOTOR_KNOB_PULL;HAL_GPIO_Init(BOARD_MOTOR_KNOB_PORT,&g);
    g.Pin=BOARD_MOTOR_KEY_PIN;g.Pull=BOARD_MOTOR_KEY_PULL;HAL_GPIO_Init(BOARD_MOTOR_KEY_PORT,&g);
#if MOTOR_DRIVER_PROFILE != 0
    BOARD_MOTOR_PWM_CLOCK_ENABLE();
#if MOTOR_DRIVER_PROFILE != 3
    HAL_GPIO_WritePin(BOARD_MOTOR_DIR_PORT,BOARD_MOTOR_IN1|BOARD_MOTOR_IN2,GPIO_PIN_RESET);
#if MOTOR_DRIVER_PROFILE == 2
    HAL_GPIO_WritePin(BOARD_MOTOR_DIR_PORT,BOARD_MOTOR_STBY,GPIO_PIN_RESET);
#endif
    g.Pin=BOARD_MOTOR_IN1|BOARD_MOTOR_IN2;
#if MOTOR_DRIVER_PROFILE == 2
    g.Pin|=BOARD_MOTOR_STBY;
#endif
    g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(BOARD_MOTOR_DIR_PORT,&g);
#endif
    s_pwm.Instance=BOARD_MOTOR_PWM_TIM;s_pwm.Init.Prescaler=BOARD_APB1_TIMER_KERNEL_HZ/BOARD_MOTOR_PWM_COUNTER_HZ-1U;s_pwm.Init.Period=BOARD_MOTOR_PWM_PERIOD-1U;
    s_pwm.Init.CounterMode=TIM_COUNTERMODE_UP;s_pwm.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    if(HAL_TIM_PWM_Init(&s_pwm)!=HAL_OK)return HAL_ERROR;
    oc.OCMode=TIM_OCMODE_PWM1;oc.Pulse=0;oc.OCPolarity=TIM_OCPOLARITY_HIGH;oc.OCFastMode=TIM_OCFAST_DISABLE;
    if(HAL_TIM_PWM_ConfigChannel(&s_pwm,&oc,TIM_CHANNEL_1)!=HAL_OK || HAL_TIM_PWM_Start(&s_pwm,TIM_CHANNEL_1)!=HAL_OK)return HAL_ERROR;
    g.Pin=BOARD_MOTOR_PWM_PIN;
#if MOTOR_DRIVER_PROFILE == 3
    if(HAL_TIM_PWM_ConfigChannel(&s_pwm,&oc,TIM_CHANNEL_2)!=HAL_OK || HAL_TIM_PWM_Start(&s_pwm,TIM_CHANNEL_2)!=HAL_OK)return HAL_ERROR;
    g.Pin|=BOARD_MOTOR_PWM2_PIN;
#endif
    g.Mode=GPIO_MODE_AF_PP;g.Alternate=BOARD_MOTOR_PWM_AF;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(BOARD_MOTOR_PWM_PORT,&g);
#endif
    s_tick.Instance=BOARD_MOTOR_TICK_TIM;s_tick.Init.Prescaler=BOARD_APB1_TIMER_KERNEL_HZ/BOARD_MOTOR_TICK_COUNTER_HZ-1U;s_tick.Init.Period=BOARD_MOTOR_TICK_COUNTER_HZ/BOARD_MOTOR_TICK_HZ-1U;
    s_tick.Init.CounterMode=TIM_COUNTERMODE_UP;s_tick.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
    if(HAL_TIM_Base_Init(&s_tick)!=HAL_OK)return HAL_ERROR;
    __HAL_TIM_CLEAR_FLAG(&s_tick,TIM_FLAG_UPDATE);
    HAL_NVIC_SetPriority(BOARD_MOTOR_TICK_IRQn,BOARD_MOTOR_TICK_PRIORITY,0);s_ready=1;
    HAL_NVIC_EnableIRQ(BOARD_MOTOR_TICK_IRQn);
    if(HAL_TIM_Base_Start_IT(&s_tick)!=HAL_OK){s_ready=0;HAL_NVIC_DisableIRQ(BOARD_MOTOR_TICK_IRQn);return HAL_ERROR;}
    return HAL_OK;
}
void BoardMotor_Irq(void)
{
    if(!s_ready)return;
    if(__HAL_TIM_GET_FLAG(&s_tick,TIM_FLAG_UPDATE) && __HAL_TIM_GET_IT_SOURCE(&s_tick,TIM_IT_UPDATE)) {
        __HAL_TIM_CLEAR_IT(&s_tick,TIM_IT_UPDATE);
        if(s_ready)MotorService_Tick1ms();
    }
}
