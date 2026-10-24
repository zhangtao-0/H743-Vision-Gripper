/* STM32H743 HAL backend. New MCU families replace this file via the target selector. */
#include "board_gpio.h"
typedef struct {
 USART_TypeDef *instance;
 GPIO_TypeDef *tx_port,*rx_port;
 uint16_t tx_pin,rx_pin;
 uint32_t af,pull,speed,baud,rcc_selection,rcc_group,priority;
 IRQn_Type irq;
 void (*clock_enable)(void);
 void (*clock_disable)(void);
} UartHardware;
#define UART_CLOCKS(role,handle) \
 static void ClockOn_##role(void){BOARD_UART_##role##_CLOCK_ENABLE();} \
 static void ClockOff_##role(void){BOARD_UART_##role##_CLOCK_DISABLE();}
BOARD_UART_BINDINGS(UART_CLOCKS)
#undef UART_CLOCKS
#define UART_HW(role,handle) [BOARD_UART_##role]={BOARD_UART_##role##_INSTANCE, \
 BOARD_UART_##role##_TX_PORT,BOARD_UART_##role##_RX_PORT, \
 BOARD_UART_##role##_TX_PIN,BOARD_UART_##role##_RX_PIN,BOARD_UART_##role##_AF, \
 BOARD_UART_##role##_PULL,BOARD_UART_##role##_SPEED,BOARD_UART_##role##_BAUD, \
 BOARD_UART_##role##_RCC_SELECTION,BOARD_UART_##role##_RCC_GROUP,BOARD_UART_##role##_PRIORITY, \
 BOARD_UART_##role##_IRQn,ClockOn_##role,ClockOff_##role},
static const UartHardware s_uart_hardware[BOARD_UART_COUNT]={BOARD_UART_BINDINGS(UART_HW)};
#undef UART_HW
static const UartHardware *Hardware(UART_HandleTypeDef *uart){
 unsigned i;if(!uart)return 0;
 for(i=0;i<BOARD_UART_COUNT;i++)if(uart==BoardResource_GetUart((BoardUartRole)i)&&uart->Instance==s_uart_hardware[i].instance)return &s_uart_hardware[i];
 return 0;
}
BoardStatus BoardUart_ConfigureHardware(void){
 unsigned i,j;
 /* Validate the complete table before enabling any peripheral. */
 for(i=0;i<BOARD_UART_COUNT;i++){
  const UartHardware *c=&s_uart_hardware[i];
  if(!c->instance||!c->tx_port||!c->rx_port||!c->tx_pin||!c->rx_pin||!c->baud||!BoardResource_GetUart((BoardUartRole)i))return BOARD_ERROR;
  for(j=0;j<i;j++)if(c->instance==s_uart_hardware[j].instance||c->irq==s_uart_hardware[j].irq)return BOARD_ERROR;
 }
 for(i=0;i<BOARD_UART_COUNT;i++){
  UART_HandleTypeDef *u=BoardResource_GetUart((BoardUartRole)i);
  u->Instance=s_uart_hardware[i].instance;u->Init.BaudRate=s_uart_hardware[i].baud;
  u->Init.WordLength=BOARD_UART_WORD_LENGTH;u->Init.StopBits=BOARD_UART_STOP_BITS;
  u->Init.Parity=BOARD_UART_PARITY;u->Init.Mode=BOARD_UART_MODE;u->Init.HwFlowCtl=BOARD_UART_FLOW_CONTROL;
  u->Init.OverSampling=UART_OVERSAMPLING_16;u->Init.OneBitSampling=UART_ONE_BIT_SAMPLE_DISABLE;
  u->Init.ClockPrescaler=UART_PRESCALER_DIV1;u->AdvancedInit.AdvFeatureInit=UART_ADVFEATURE_NO_INIT;
  if(HAL_UART_Init(u)!=HAL_OK||HAL_UARTEx_SetTxFifoThreshold(u,UART_TXFIFO_THRESHOLD_1_8)!=HAL_OK||
   HAL_UARTEx_SetRxFifoThreshold(u,UART_RXFIFO_THRESHOLD_1_8)!=HAL_OK||HAL_UARTEx_DisableFifoMode(u)!=HAL_OK)return BOARD_ERROR;
 }
 return BoardUart_InitAll();
}
BoardStatus BoardUart_HardwareMspInit(UART_HandleTypeDef *u){
 const UartHardware *c=Hardware(u);GPIO_InitTypeDef g={0};RCC_PeriphCLKInitTypeDef clock={0};
 if(!c)return BOARD_ERROR;
 clock.PeriphClockSelection=c->rcc_selection;
 if(c->rcc_group==1U)clock.Usart16ClockSelection=RCC_USART16CLKSOURCE_D2PCLK2;
 else if(c->rcc_group==2U)clock.Usart234578ClockSelection=RCC_USART234578CLKSOURCE_D2PCLK1;
 else return BOARD_ERROR;
 if(HAL_RCCEx_PeriphCLKConfig(&clock)!=HAL_OK)return BOARD_ERROR;
 c->clock_enable();BoardGpio_EnableClock(c->tx_port);BoardGpio_EnableClock(c->rx_port);
 g.Mode=GPIO_MODE_AF_PP;g.Pull=c->pull;g.Speed=c->speed;g.Alternate=c->af;
 g.Pin=c->tx_pin;HAL_GPIO_Init(c->tx_port,&g);g.Pin=c->rx_pin;HAL_GPIO_Init(c->rx_port,&g);
 HAL_NVIC_SetPriority(c->irq,c->priority,0);HAL_NVIC_EnableIRQ(c->irq);return BOARD_OK;
}
void BoardUart_HardwareMspDeInit(UART_HandleTypeDef *u){
 const UartHardware *c=Hardware(u);if(!c)return;
 HAL_NVIC_DisableIRQ(c->irq);c->clock_disable();
 HAL_GPIO_DeInit(c->tx_port,c->tx_pin);HAL_GPIO_DeInit(c->rx_port,c->rx_pin);
}
void BoardUart_Irq(BoardUartRole role){UART_HandleTypeDef *u=BoardResource_GetUart(role);if(u)HAL_UART_IRQHandler(u);}
