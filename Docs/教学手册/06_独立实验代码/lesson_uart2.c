/* Independent USART2 loopback lesson; never add alongside production
 * USART2_IRQHandler / HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback.
 * Use a separate target with HAL/CMSIS/startup/linker, HAL_Init and correct
 * SystemClock_Config. Disconnect the STS adapter before TX/RX loopback.
 * This lesson uses 115200; production servo USART2 starts at 1000000. */
#include "platform_hal.h"
#define LESSON_RING 64U
UART_HandleTypeDef lesson_uart2;
static uint8_t s_byte,s_ring[LESSON_RING];
static volatile uint16_t s_head,s_tail;
volatile uint32_t lesson_rx_bytes,lesson_rx_dropped,lesson_uart_errors,lesson_rearm_errors;
HAL_StatusTypeDef Lesson_Uart2_Init(void)
{
    GPIO_InitTypeDef pin={0};RCC_PeriphCLKInitTypeDef clock={0};
    clock.PeriphClockSelection=RCC_PERIPHCLK_USART2;
    clock.Usart234578ClockSelection=RCC_USART234578CLKSOURCE_D2PCLK1;
    if(HAL_RCCEx_PeriphCLKConfig(&clock)!=HAL_OK)return HAL_ERROR;
    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_USART2_CLK_ENABLE();
    pin.Pin=GPIO_PIN_2|GPIO_PIN_3;pin.Mode=GPIO_MODE_AF_PP;
    pin.Pull=GPIO_NOPULL;pin.Speed=GPIO_SPEED_FREQ_HIGH;pin.Alternate=GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA,&pin);
    lesson_uart2.Instance=USART2;lesson_uart2.Init.BaudRate=115200U;
    lesson_uart2.Init.WordLength=UART_WORDLENGTH_8B;lesson_uart2.Init.StopBits=UART_STOPBITS_1;
    lesson_uart2.Init.Parity=UART_PARITY_NONE;lesson_uart2.Init.Mode=UART_MODE_TX_RX;
    lesson_uart2.Init.HwFlowCtl=UART_HWCONTROL_NONE;lesson_uart2.Init.OverSampling=UART_OVERSAMPLING_16;
    lesson_uart2.Init.OneBitSampling=UART_ONE_BIT_SAMPLE_DISABLE;lesson_uart2.Init.ClockPrescaler=UART_PRESCALER_DIV1;
    lesson_uart2.AdvancedInit.AdvFeatureInit=UART_ADVFEATURE_NO_INIT;
    if(HAL_UART_Init(&lesson_uart2)!=HAL_OK)return HAL_ERROR;
    if(HAL_UARTEx_DisableFifoMode(&lesson_uart2)!=HAL_OK)return HAL_ERROR;
    HAL_NVIC_SetPriority(USART2_IRQn,2,0);HAL_NVIC_EnableIRQ(USART2_IRQn);
    return HAL_UART_Receive_IT(&lesson_uart2,&s_byte,1U);
}
void USART2_IRQHandler(void){HAL_UART_IRQHandler(&lesson_uart2);}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if(uart==&lesson_uart2){
        uint16_t next=(uint16_t)((s_head+1U)%LESSON_RING);lesson_rx_bytes++;
        if(next==s_tail)lesson_rx_dropped++;
        else{s_ring[s_head]=s_byte;__DMB();s_head=next;}
        if(HAL_UART_Receive_IT(uart,&s_byte,1U)!=HAL_OK)lesson_rearm_errors++;
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if(uart==&lesson_uart2){
        lesson_uart_errors++;__HAL_UART_CLEAR_OREFLAG(uart);
        if(uart->RxState==HAL_UART_STATE_READY && HAL_UART_Receive_IT(uart,&s_byte,1U)!=HAL_OK)lesson_rearm_errors++;
    }
}
int Lesson_Uart2_Pop(uint8_t *out)
{
    uint32_t saved;uint16_t head,tail;int available=0;if(!out)return 0;
    saved=__get_PRIMASK();__disable_irq();
    tail=s_tail;
    head=s_head;
    if(tail!=head){
        *out=s_ring[tail];
        s_tail=(uint16_t)((tail+1U)%LESSON_RING);
        available=1;
    }
    __set_PRIMASK(saved);return available;
}
HAL_StatusTypeDef Lesson_Uart2_Send(const uint8_t *data,uint16_t size)
{
    uint32_t timeout=((uint32_t)size*10000U+115199U)/115200U+5U;
    if(!data || !size)return HAL_ERROR;
    return HAL_UART_Transmit(&lesson_uart2,(uint8_t *)data,size,timeout);
}
