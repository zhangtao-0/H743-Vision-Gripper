from pathlib import Path
import re
H=Path(__file__).resolve().parents[3];A=H/'project/App'
def put(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf8')
p=A/'platform/board_uart.h';s=p.read_text(encoding='utf8');s=s.replace('#include "board_resources.h"','''#include <stdint.h>
typedef enum {
 BOARD_UART_SERVO=0, BOARD_UART_HOST, BOARD_UART_BLUETOOTH, BOARD_UART_COUNT
} BoardUartRole;''');s=s.replace('/* 校验角色表非空；实际外设初始化由 CubeMX 生成函数完成。 */','/* Validate configured and initialized role bindings. */\nBoardStatus BoardUart_ConfigureHardware(void);');put(p,s)
put(A/'platform/board_resources.h','''#ifndef BOARD_RESOURCES_H
#define BOARD_RESOURCES_H
/* HAL-only internals; business code should include board_uart.h instead. */
#include "platform_hal.h"
#include "board_uart.h"
UART_HandleTypeDef *BoardResource_GetUart(BoardUartRole role);
BoardStatus BoardUart_HardwareMspInit(UART_HandleTypeDef *uart);
void BoardUart_HardwareMspDeInit(UART_HandleTypeDef *uart);
void BoardUart_Irq(BoardUartRole role);
#endif
''')
put(A/'platform/board_resources.c','''#include "board_resources.h"
#include "board_config.h"
#define UART_EXTERN(role,handle) extern UART_HandleTypeDef handle;
BOARD_UART_BINDINGS(UART_EXTERN)
#undef UART_EXTERN
#define UART_BIND(role,handle) [BOARD_UART_##role] = &handle,
static UART_HandleTypeDef *const s_uart[BOARD_UART_COUNT]={BOARD_UART_BINDINGS(UART_BIND)};
#undef UART_BIND
UART_HandleTypeDef *BoardResource_GetUart(BoardUartRole role)
{return (unsigned)role<BOARD_UART_COUNT?s_uart[role]:0;}
''')
p=A/'platform/board_uart.c';s=p.read_text(encoding='utf8').replace('#include "board_config.h"','#include "board_config.h"\n#include "board_resources.h"');s=s.replace('''        BOARD_UART_SERVO_BAUD, BOARD_UART_HOST_BAUD, BOARD_UART_BLUETOOTH_BAUD''','''#define UART_BAUD(role,handle) [BOARD_UART_##role]=BOARD_UART_##role##_BAUD,
        BOARD_UART_BINDINGS(UART_BAUD)
#undef UART_BAUD''');s+='''\n/* Byte-level host regressions may omit MCU hardware setup; production always includes it. */
#ifndef PLATFORM_TEST_UART_BYTE_ONLY
#include PLATFORM_UART_HW_BACKEND
#endif
''';put(p,s)
backend='''/* STM32H743 HAL backend. New MCU families replace this file via the target selector. */
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
#define UART_CLOCKS(role,handle) \\
 static void ClockOn_##role(void){BOARD_UART_##role##_CLOCK_ENABLE();} \\
 static void ClockOff_##role(void){BOARD_UART_##role##_CLOCK_DISABLE();}
BOARD_UART_BINDINGS(UART_CLOCKS)
#undef UART_CLOCKS
#define UART_HW(role,handle) [BOARD_UART_##role]={BOARD_UART_##role##_INSTANCE, \\
 BOARD_UART_##role##_TX_PORT,BOARD_UART_##role##_RX_PORT, \\
 BOARD_UART_##role##_TX_PIN,BOARD_UART_##role##_RX_PIN,BOARD_UART_##role##_AF, \\
 BOARD_UART_##role##_PULL,BOARD_UART_##role##_SPEED,BOARD_UART_##role##_BAUD, \\
 BOARD_UART_##role##_RCC_SELECTION,BOARD_UART_##role##_RCC_GROUP,BOARD_UART_##role##_PRIORITY, \\
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
'''
put(A/'platform/mcu/stm32h743/uart_backend.inc',backend)
# Route Core startup, MSP and interrupts through the same role table.
p=H/'project/Core/Src/main.c';s=p.read_text(encoding='utf8').replace('#include "board_peripherals.h"','#include "board_peripherals.h"\n#include "board_uart.h"')
s=re.sub(r'UART_HandleTypeDef huart1;\s*UART_HandleTypeDef huart2;\s*UART_HandleTypeDef huart3;', '#define UART_DEFINE(role,handle) UART_HandleTypeDef handle;\nBOARD_UART_BINDINGS(UART_DEFINE)\n#undef UART_DEFINE',s)
s=re.sub(r'^static void MX_USART[123]_UART_Init\(void\);\n','',s,flags=re.M)
s=s.replace('  MX_USART1_UART_Init();\n  MX_USART2_UART_Init();\n  MX_USART3_UART_Init();','  if(BoardUart_ConfigureHardware()!=BOARD_OK) Error_Handler();')
a=s.index('/**\n  * @brief USART1 Initialization Function');b=s.index('/**',s.index('static void MX_GPIO_Init(void)\n{')-180) if False else s.index('static void MX_GPIO_Init(void)\n{')
# Remove UART implementation blocks and leave GPIO implementation; comments around it are restored below.
s=s[:a]+ '/* GPIO startup delegates to configured device adapters. */\n'+s[b:]
# Old Cube GPIO startup duplicated the LED/LCD pins; those are now initialized by their owners.
a=s.index('static void MX_GPIO_Init(void)\n{');b=s.index('/* USER CODE BEGIN 4 */',a)
s=s[:a]+'''static void MX_GPIO_Init(void)
{
  /* LED, key, LCD and serial adapters enable/configure their own GPIO ports. */
}

'''+s[b:]
for field in 'MNPQR':s=s.replace(f'RCC_OscInitStruct.PLL.PLL{field} = '+{'M':'4','N':'128','P':'2','Q':'2','R':'2'}[field]+';',f'RCC_OscInitStruct.PLL.PLL{field} = BOARD_SYS_PLL_{field};')
put(p,s)
p=H/'project/Core/Src/stm32h7xx_hal_msp.c';s=p.read_text(encoding='utf8');s=s.replace('#include "main.h"','#include "main.h"\n#include "board_resources.h"');a=s.index('void HAL_UART_MspInit(');s=s[:a]+'''void HAL_UART_MspInit(UART_HandleTypeDef *uart)
{
 if(BoardUart_HardwareMspInit(uart)!=BOARD_OK) Error_Handler();
}
void HAL_UART_MspDeInit(UART_HandleTypeDef *uart)
{
 BoardUart_HardwareMspDeInit(uart);
}
''';put(p,s)
p=H/'project/Core/Src/stm32h7xx_it.c';s=p.read_text(encoding='utf8');s=s.replace('#include "stm32h7xx_it.h"','#include "stm32h7xx_it.h"\n#include "board_config.h"\n#include "board_resources.h"').replace('void TIM6_DAC_IRQHandler(void)','void BOARD_MOTOR_TICK_HANDLER(void)')
a=s.index('extern UART_HandleTypeDef huart1;');b=s.index('/* USER CODE END 1 */',a)
s=s[:a]+'''#define UART_IRQ(role,handle) void BOARD_UART_##role##_HANDLER(void){BoardUart_Irq(BOARD_UART_##role);}
BOARD_UART_BINDINGS(UART_IRQ)
#undef UART_IRQ
'''+s[b:];put(p,s)
# Register new portable interface implementations in the existing IAR group.
p=H/'project/EWARM/H743_VisionGripper.ewp';s=p.read_text(encoding='utf8');needle=r'<name>$PROJ_DIR$\..\App\platform\board_resources.c</name>';assert needle in s
pos=s.index('</file>',s.index(needle))+len('</file>');s=s[:pos]+'''\n        <file><name>$PROJ_DIR$/../App/platform/board_gpio.c</name></file>
        <file><name>$PROJ_DIR$/../App/platform/board_nv_storage.c</name></file>'''+s[pos:];put(p,s)
print('UART role mapping now drives hardware init/MSP/IRQ; Core duplicates removed')
