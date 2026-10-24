"""Compile actual role table/H743 UART backend with an instrumented HAL; no board IO."""
from pathlib import Path
import tempfile,subprocess,json,re
H=Path(__file__).resolve().parents[1];A=H/'project/App';E=H/'Docs/解耦与移植/20261024_配置分层重构'
GCC=r'C:\Dev-Cpp\MinGW64\bin\gcc.exe'
HAL=r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
typedef enum{HAL_OK,HAL_ERROR,HAL_BUSY,HAL_TIMEOUT}HAL_StatusTypeDef;
typedef struct{unsigned id;}GPIO_TypeDef;
typedef struct{unsigned id;}USART_TypeDef;
extern GPIO_TypeDef ga,gb;extern USART_TypeDef us1,us2,us3;
#define GPIOA (&ga)
#define GPIOB (&gb)
#define USART1 (&us1)
#define USART2 (&us2)
#define USART3 (&us3)
typedef int IRQn_Type;
#define USART1_IRQn 1
#define USART2_IRQn 2
#define USART3_IRQn 3
#define GPIO_AF7_USART1 7U
#define GPIO_AF7_USART2 7U
#define GPIO_AF7_USART3 7U
#define GPIO_MODE_AF_PP 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 2U
#define UART_WORDLENGTH_8B 8U
#define UART_STOPBITS_1 1U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 3U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 16U
#define UART_ONE_BIT_SAMPLE_DISABLE 0U
#define UART_PRESCALER_DIV1 1U
#define UART_ADVFEATURE_NO_INIT 0U
#define UART_TXFIFO_THRESHOLD_1_8 1U
#define UART_RXFIFO_THRESHOLD_1_8 1U
#define HAL_UART_STATE_READY 0U
#define RCC_PERIPHCLK_USART1 1U
#define RCC_PERIPHCLK_USART2 2U
#define RCC_PERIPHCLK_USART3 3U
#define RCC_USART16CLKSOURCE_D2PCLK2 12U
#define RCC_USART234578CLKSOURCE_D2PCLK1 34U
extern unsigned gpio_clocks[2],uart_clocks[4];
#define __HAL_RCC_GPIOA_CLK_ENABLE() (++gpio_clocks[0])
#define __HAL_RCC_GPIOB_CLK_ENABLE() (++gpio_clocks[1])
#define __HAL_RCC_USART1_CLK_ENABLE() (++uart_clocks[1])
#define __HAL_RCC_USART2_CLK_ENABLE() (++uart_clocks[2])
#define __HAL_RCC_USART3_CLK_ENABLE() (++uart_clocks[3])
#define __HAL_RCC_USART1_CLK_DISABLE() (--uart_clocks[1])
#define __HAL_RCC_USART2_CLK_DISABLE() (--uart_clocks[2])
#define __HAL_RCC_USART3_CLK_DISABLE() (--uart_clocks[3])
#define __HAL_UART_CLEAR_OREFLAG(u) ((void)(u))
typedef struct{uint32_t Pin,Mode,Pull,Speed,Alternate;}GPIO_InitTypeDef;
typedef struct{uint32_t PeriphClockSelection,Usart16ClockSelection,Usart234578ClockSelection;}RCC_PeriphCLKInitTypeDef;
typedef struct{USART_TypeDef *Instance;struct{uint32_t BaudRate,WordLength,StopBits,Parity,Mode,HwFlowCtl,OverSampling,OneBitSampling,ClockPrescaler;}Init;struct{uint32_t AdvFeatureInit;}AdvancedInit;uint32_t RxState;}UART_HandleTypeDef;
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *,uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UARTEx_SetTxFifoThreshold(UART_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_UARTEx_SetRxFifoThreshold(UART_HandleTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *);
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
void HAL_GPIO_DeInit(GPIO_TypeDef *,uint16_t);
void HAL_NVIC_SetPriority(IRQn_Type,uint32_t,uint32_t);
void HAL_NVIC_EnableIRQ(IRQn_Type);
void HAL_NVIC_DisableIRQ(IRQn_Type);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
#endif
'''
HAL=HAL.replace('#endif','\n'.join(f'#define GPIO_PIN_{i} (1U<<{i})' for i in range(16))+'\n#endif')
TEST=r'''
#include <assert.h>
#include <stdio.h>
#include "board_config.h"
#include "board_resources.h"
GPIO_TypeDef ga={0},gb={1};USART_TypeDef us1={1},us2={2},us3={3};
unsigned gpio_clocks[2],uart_clocks[4];
#define DEFINE(role,h) UART_HandleTypeDef h;
BOARD_UART_BINDINGS(DEFINE)
#undef DEFINE
static unsigned inits,gpios,irq_on[4],irq_prio[4],clock_group[4],deinit_gpio,irq_calls;
static UART_HandleTypeDef *last_irq;
static uint16_t pins[6];
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *u){inits++;u->RxState=HAL_UART_STATE_READY;return BoardUart_HardwareMspInit(u)==BOARD_OK?HAL_OK:HAL_ERROR;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*u){u->RxState=0;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*u,uint8_t*p,uint16_t n,uint32_t t){(void)u;(void)p;(void)n;(void)t;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)p;assert(n==1);u->RxState=1;return HAL_OK;}
HAL_StatusTypeDef HAL_UARTEx_SetTxFifoThreshold(UART_HandleTypeDef*u,uint32_t v){(void)u;(void)v;return HAL_OK;}
HAL_StatusTypeDef HAL_UARTEx_SetRxFifoThreshold(UART_HandleTypeDef*u,uint32_t v){(void)u;(void)v;return HAL_OK;}
HAL_StatusTypeDef HAL_UARTEx_DisableFifoMode(UART_HandleTypeDef*u){(void)u;return HAL_OK;}
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*c){clock_group[c->PeriphClockSelection]=c->Usart16ClockSelection?c->Usart16ClockSelection:c->Usart234578ClockSelection;return HAL_OK;}
void HAL_GPIO_Init(GPIO_TypeDef*p,GPIO_InitTypeDef*g){assert(p==GPIOA||p==GPIOB);assert(g->Pull==0&&g->Alternate==7);pins[gpios++]=g->Pin;}
void HAL_GPIO_DeInit(GPIO_TypeDef*p,uint16_t pin){(void)p;(void)pin;deinit_gpio++;}
void HAL_NVIC_SetPriority(IRQn_Type q,uint32_t p,uint32_t s){assert(!s);irq_prio[q]=p;}
void HAL_NVIC_EnableIRQ(IRQn_Type q){irq_on[q]=1;}
void HAL_NVIC_DisableIRQ(IRQn_Type q){irq_on[q]=0;}
void HAL_UART_IRQHandler(UART_HandleTypeDef*u){last_irq=u;irq_calls++;}
IRQ_DEFINITIONS
int main(void){
#ifdef TEST_DUPLICATE
 assert(BoardUart_ConfigureHardware()==BOARD_ERROR&&inits==0&&gpios==0);puts("PASS duplicate UART/IRQ rejected before hardware effects");
#else
 assert(BoardUart_ConfigureHardware()==BOARD_OK&&inits==3&&gpios==6);
 assert(BoardUart_GetBaud(BOARD_UART_HOST)==115200&&BoardUart_GetBaud(BOARD_UART_SERVO)==1000000&&BoardUart_GetBaud(BOARD_UART_BLUETOOTH)==9600);
 UART_HandleTypeDef *host=BoardResource_GetUart(BOARD_UART_HOST);
#ifdef TEST_SWAP
 assert(host->Instance==USART2&&pins[2]==GPIO_PIN_2&&pins[3]==GPIO_PIN_3&&clock_group[2]==34);
#else
 assert(host->Instance==USART1&&pins[2]==GPIO_PIN_9&&pins[3]==GPIO_PIN_10&&clock_group[1]==12);
#endif
 assert(irq_prio[BOARD_UART_HOST_IRQn]==0&&irq_on[BOARD_UART_HOST_IRQn]);
 BOARD_UART_HOST_HANDLER();assert(irq_calls==1&&last_irq==host);
 BoardUart_HardwareMspDeInit(host);assert(deinit_gpio==2&&!irq_on[BOARD_UART_HOST_IRQn]);
 assert(!BoardResource_GetUart((BoardUartRole)99));assert(BoardUart_Transmit((BoardUartRole)99,(const uint8_t*)"a",1,1)==BOARD_ERROR);
 puts("PASS configured role drives baud/GPIO/RCC/NVIC/IRQ consistently, no physical handles in business");
#endif
 return 0;
}
'''
def main():
 serial=(A/'platform/boards/lxb743zi_p1/board_serial_cfg.h').read_text(encoding='utf8')
 irq=(H/'project/Core/Src/stm32h7xx_it.c').read_text(encoding='utf8');irq=irq[irq.index('#define UART_IRQ('):irq.index('#undef UART_IRQ')+len('#undef UART_IRQ')]
 results=[]
 with tempfile.TemporaryDirectory() as folder:
  p=Path(folder);(p/'stm32h7xx_hal.h').write_text(HAL);(p/'test.c').write_text(TEST.replace('IRQ_DEFINITIONS',irq))
  for mode in ['default','swap','duplicate']:
   extra=''
   if mode=='swap':
    for role,other in [('HOST','SERVO'),('SERVO','HOST')]:
     for field in ['INSTANCE','TX_PORT','TX_PIN','RX_PORT','RX_PIN','AF','IRQn','HANDLER','RCC_SELECTION','RCC_GROUP']:
      value=re.search(r'^#define BOARD_UART_'+other+'_'+field+r'\s+(.+)$',serial,re.M)[1];extra+=f'#undef BOARD_UART_{role}_{field}\n#define BOARD_UART_{role}_{field} {value}\n'
     for field in ['CLOCK_ENABLE','CLOCK_DISABLE']:
      value=re.search(r'^#define BOARD_UART_'+other+'_'+field+r'\(\)\s+(.+)$',serial,re.M)[1];extra+=f'#undef BOARD_UART_{role}_{field}\n#define BOARD_UART_{role}_{field}() {value}\n'
   if mode=='duplicate':extra='#undef BOARD_UART_HOST_INSTANCE\n#define BOARD_UART_HOST_INSTANCE USART2\n'
   (p/'test_board.h').write_text('#include "boards/lxb743zi_p1/board_profile.h"\n'+extra)
   (p/'test_target.h').write_text('#define PLATFORM_MCU_FAMILY_H7 1\n#define PLATFORM_HAL_HEADER "stm32h7xx_hal.h"\n#define APP_BOARD_PROFILE_HEADER "test_board.h"\n#define PLATFORM_UART_HW_BACKEND "mcu/stm32h743/uart_backend.inc"\n')
   cmd=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-DSTM32H743xx','-DAPP_TARGET_CONFIG_HEADER="test_target.h"']
   if mode!='default':cmd+=['-DTEST_'+mode.upper()]
   for inc in [p,A/'platform',A/'config']:cmd+=['-I',str(inc)]
   cmd += [str(p/'test.c'),str(A/'platform/board_resources.c'),str(A/'platform/board_uart.c'),str(A/'platform/board_gpio.c'),'-o',str(p/'test.exe')]
   b=subprocess.run(cmd,capture_output=True,text=True)
   if b.returncode:raise RuntimeError(mode+'\n'+b.stdout+b.stderr)
   r=subprocess.run([str(p/'test.exe')],capture_output=True,text=True);print(mode,r.stdout,end='');r.check_returncode();results.append({'mode':mode,'exit_code':r.returncode,'stdout':r.stdout})
 (E/'UART配置一致性测试.json').write_text(json.dumps({'scope':'actual H743 UART adapter + fake HAL; default/remap/invalid profile; no device IO','passed':True,'cases':results},ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':main()
