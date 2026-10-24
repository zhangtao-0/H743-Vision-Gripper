from pathlib import Path
import re
H=Path(__file__).resolve().parents[3];A=H/'project/App';B=A/'platform/boards/lxb743zi_p1'
def put(p,s):p.write_text(s,encoding='utf8')
p=A/'platform/board_uart.h';s=p.read_text(encoding='utf8').replace('BoardStatus BoardUart_InitAll(void);','BoardStatus BoardUart_InitAll(void);\nBoardStatus BoardUart_TransmitIT(BoardUartRole role,const uint8_t *data,uint16_t len);');put(p,s)
p=A/'platform/board_uart.c';s=p.read_text(encoding='utf8');pos=s.index('BoardStatus BoardUart_SetBaud(');s=s[:pos]+'''BoardStatus BoardUart_TransmitIT(BoardUartRole role,const uint8_t *data,uint16_t len)
{
 UART_HandleTypeDef *uart=BoardUart_Handle(role);
 if(!uart||!data||!len)return BOARD_ERROR;
 return BoardUart_Status(HAL_UART_Transmit_IT(uart,(uint8_t *)data,len));
}

'''+s[pos:];put(p,s)
for filename,prefix,role in [('bsp/bsp_bluetooth','BLUETOOTH','BLUETOOTH'),('protocol/vofa_protocol','VOFA','HOST')]:
 p=A/(filename+'.h');s=p.read_text(encoding='utf8').replace('#include "board_resources.h"','#include "board_uart.h"');s=re.sub(r'^#define '+prefix+r'_UART_HANDLE[^\n]*\n','',s,flags=re.M);put(p,s)
 p=A/(filename+'.c');s=p.read_text(encoding='utf8')
 if prefix=='BLUETOOTH':
  s=s.replace('(huart != &BLUETOOTH_UART_HANDLE)','(!BoardUart_IsInstance(huart,BOARD_UART_BLUETOOTH))')
  s=s.replace('HAL_StatusTypeDef status;','BoardStatus status;').replace('status == HAL_OK','status == BOARD_OK').replace('status == HAL_BUSY','status == BOARD_BUSY').replace('status != HAL_OK','status != BOARD_OK').replace('status != HAL_BUSY','status != BOARD_BUSY')
  s=s.replace('    __HAL_UART_CLEAR_OREFLAG(&BLUETOOTH_UART_HANDLE);\n    status = HAL_UART_Receive_IT(&BLUETOOTH_UART_HANDLE, (uint8_t *)&s_bt.rx_byte, 1U);','    status = BoardUart_StartRxIT(BOARD_UART_BLUETOOTH,(uint8_t *)&s_bt.rx_byte);')
  s=s.replace('HAL_UART_Transmit_IT(&BLUETOOTH_UART_HANDLE, s_tx.active_buf, len)','BoardUart_TransmitIT(BOARD_UART_BLUETOOTH,s_tx.active_buf,len)')
 else:
  s=s.replace('huart == &VOFA_UART_HANDLE','BoardUart_IsInstance(huart,BOARD_UART_HOST)')
  s=s.replace('    __HAL_UART_CLEAR_OREFLAG(&VOFA_UART_HANDLE);\n    status = HAL_UART_Receive_IT(&VOFA_UART_HANDLE, (uint8_t *)&s_vofa.rx_byte, 1U);','''    switch(BoardUart_StartRxIT(BOARD_UART_HOST,(uint8_t *)&s_vofa.rx_byte)) {
     case BOARD_OK:status=HAL_OK;break;case BOARD_BUSY:status=HAL_BUSY;break;
     case BOARD_TIMEOUT:status=HAL_TIMEOUT;break;default:status=HAL_ERROR;break;
    }''')
  s=s.replace('HAL_UART_Transmit_IT(&VOFA_UART_HANDLE, s_tx.active_buf, len) != HAL_OK','BoardUart_TransmitIT(BOARD_UART_HOST,s_tx.active_buf,len) != BOARD_OK')
 put(p,s)
# A configurable timer period must preserve the public permille output contract.
p=A/'platform/board_motor.c';s=p.read_text(encoding='utf8').replace('duty=(uint32_t)(v<0 ? -v : v);','duty=(uint32_t)(((uint64_t)(v<0 ? -v : v)*BOARD_MOTOR_PWM_PERIOD)/1000U);');put(p,s)
p=B/'board_io_cfg.h';s=p.read_text(encoding='utf8');s=re.sub(r'^#define BOARD_(KEY|LED)_CLOCK_ENABLE\(\).*\n','',s,flags=re.M);put(p,s)
p=B/'board_motion_cfg.h';s=p.read_text(encoding='utf8').replace('#define BOARD_PWM_KERNEL_HZ 200000000UL','#define BOARD_PWM_KERNEL_HZ BOARD_APB1_TIMER_KERNEL_HZ');pos=s.rindex('#endif');s=s[:pos]+'''/* The service API is a1ms tick, and PWM servo pulse units are microseconds. */
#if BOARD_MOTOR_TICK_HZ != 1000U
#error "MotorService_Tick1ms requires a1000Hz scheduler"
#endif
#if BOARD_MOTOR_PWM_PERIOD == 0 || BOARD_MOTOR_PWM_COUNTER_HZ % BOARD_MOTOR_PWM_PERIOD
#error "Invalid motor PWM timer period"
#endif
#if BOARD_PWM_COUNTER_HZ != 1000000U
#error "PWM servo API requires a1MHz microsecond counter"
#endif
'''+s[pos:];put(p,s)
# Existing test HALs gain an async stub; wire-path tests will continue checking real queue behavior.
for name in ['run_p2_host_tests.py','run_p3_host_tests.py']:
 p=H/'Tools'/name;s=p.read_text(encoding='utf8');s=s.replace('#define HAL_UART_STATE_READY 0U','#define HAL_UART_STATE_READY 0U\nHAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);')
 key='uint32_t HAL_GetTick(void){return tick;}'
 s=s.replace(key,key+'\nHAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_ERROR;}')
 put(p,s)
p=H/'Tools/run_motor_plot_tests.py';s=p.read_text(encoding='utf8');s=s.replace('"-DSTM32H743xx"','"-DSTM32H743xx", "-DPLATFORM_TEST_UART_BYTE_ONLY=1"').replace("'-DSTM32H743xx'","'-DSTM32H743xx','-DPLATFORM_TEST_UART_BYTE_ONLY=1'")
s=s.replace("str(app/'platform/board_resources.c'),","str(app/'platform/board_resources.c'),str(app/'platform/board_uart.c'),")
s=s.replace('uint32_t HAL_GetTick(void){return tick;}','''uint32_t HAL_GetTick(void){return tick;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*u){(void)u;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*u){(void)u;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*u,const uint8_t*p,uint16_t n,uint32_t t){(void)u;(void)p;(void)n;(void)t;return HAL_ERROR;}
''');put(p,s)
p=H/'Tools/run_portability_tests.py';s=p.read_text(encoding='utf8');s=s.replace('HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);','HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);\nHAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);')
s=s.replace('HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*u)','HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_OK;}\nHAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef*u)');put(p,s)
print('Async transport stays inside role adapter; timer scaling and fixed unit contracts checked')
