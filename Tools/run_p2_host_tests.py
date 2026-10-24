"""Exercise P2 state/queue/timing boundaries with a host HAL stub; no board access."""
from pathlib import Path
import json
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GCC = Path(r"C:\Dev-Cpp\MinGW64\bin\gcc.exe")
HAL = r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
extern uint32_t key_primask;
#define __get_PRIMASK() key_primask
#define __disable_irq() (key_primask=1)
#define __set_PRIMASK(p) (key_primask=(p))
#define __DMB() __asm__ volatile ("" ::: "memory")
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef enum {GPIO_PIN_RESET, GPIO_PIN_SET} GPIO_PinState;
typedef struct {uint16_t value;} GPIO_TypeDef;
extern GPIO_TypeDef ga,gb,gc,gg;
#define GPIOA (&ga)
#define GPIOB (&gb)
#define GPIOC (&gc)
#define GPIOG (&gg)
#define GPIO_PIN_0 (1U<<0)
#define GPIO_PIN_1 (1U<<1)
#define GPIO_PIN_7 (1U<<7)
#define GPIO_PIN_12 (1U<<12)
#define GPIO_PIN_13 (1U<<13)
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define HAL_UART_STATE_READY 0U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOG_CLK_ENABLE() ((void)0)
#define __HAL_UART_CLEAR_OREFLAG(u) ((void)(u))
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
typedef struct {void *Instance; struct {uint32_t BaudRate;} Init; uint32_t RxState;} UART_HandleTypeDef;
uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(GPIO_TypeDef *,uint16_t,GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *,uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
#endif
'''
TEST = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "board_uart.h"
#include "board_resources.h"
#include "board_led.h"
#include "host_diagnostics.h"
#include "feetech_sts_port.h"
#include "bsp_key.h"
#include "ui_input.h"
GPIO_TypeDef ga={0xffff},gb={0xffff},gc={0xffff},gg={0xffff};
UART_HandleTypeDef huart1={(void*)1,{BOARD_UART_HOST_BAUD},0};
UART_HandleTypeDef huart2={(void*)2,{BOARD_UART_SERVO_BAUD},0};
UART_HandleTypeDef huart3={(void*)3,{BOARD_UART_BLUETOOTH_BAUD},0};
uint32_t key_primask;
static uint32_t tick,last_timeout,tx_len;
static uint8_t *host_rx;
static uint8_t tx_bytes[2048];
static HAL_StatusTypeDef tx_result=HAL_OK,rx_result=HAL_OK;
uint32_t HAL_GetTick(void){return tick;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_ERROR;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u){u->RxState=HAL_UART_STATE_READY;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *u){u->RxState=HAL_UART_STATE_READY;return HAL_OK;}
void HAL_GPIO_Init(GPIO_TypeDef *g,GPIO_InitTypeDef *i){(void)g;(void)i;}
void HAL_GPIO_WritePin(GPIO_TypeDef *g,uint16_t p,GPIO_PinState s){if(s)g->value|=p;else g->value&=(uint16_t)~p;}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g,uint16_t p){return (g->value&p)?GPIO_PIN_SET:GPIO_PIN_RESET;}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u,uint8_t *b,uint16_t n,uint32_t timeout){
 last_timeout=timeout;
 if(tx_result==HAL_OK && u==&huart1){assert(tx_len+n<=sizeof(tx_bytes));memcpy(tx_bytes+tx_len,b,n);tx_len+=n;}
 return tx_result;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n){
 assert(n==1);if(rx_result!=HAL_OK)return rx_result;
 if(u==&huart1)host_rx=p;u->RxState=1;return HAL_OK;
}
static void feed(uint8_t b){assert(host_rx);*host_rx=b;huart1.RxState=HAL_UART_STATE_READY;HostDiagnostics_RxComplete();}
static UiInput ui_input;static unsigned ui_next,ui_ok,ui_back,ui_shortcut;
static void Drive(unsigned ms){while(ms--){tick++;Key_Task();UiEvent e=UiInput_Sample(&ui_input,tick,Key_GetFlag(Key_Task1),Key_GetFlag(Key_Task2),Key_GetFlag(Key_Task3),Key_GetFlag(Key_Task4));if(e==UI_NEXT)ui_next++;if(e==UI_OK)ui_ok++;if(e==UI_BACK)ui_back++;if(e==UI_SHORTCUT)ui_shortcut++;}}
static void MappingTest(void){
 ga.value=gb.value=gc.value=0xffff;tick=10000;Key_Init();UiInput_Init(&ui_input);
 assert(!Input_DebugEnabled(0)&&!Input_DebugEnabled(1));
 assert(Input_DebugSet(0,1)&&Input_DebugSet(1,1));assert(Input_DebugEnabled(0)&&Input_DebugEnabled(1));
 assert(Input_Level(0,5,1,tick));assert(!Input_DebugSet(0,0));Input_Release(0);
 Input_LegacyHeld(1);assert(!Input_DebugSet(1,0));Input_LegacyHeld(0);
 assert(Input_DebugSet(0,0));assert(!Input_DebugEnabled(0)&&Input_DebugEnabled(1));assert(Input_DebugSet(1,0));

 assert(Input_Pulse(0,2,100,tick));assert(!Input_Pulse(0,2,100,tick));Drive(150);assert(ui_next==1);
 assert(Input_Pulse(1,8,900,tick));Drive(950);assert(ui_back==1);
 assert(Input_Pulse(0,12,100,tick));Drive(150);assert(ui_shortcut==1&&ui_ok==0);
 gb.value&=(uint16_t)~GPIO_PIN_12;Drive(30);assert(Key_GetFlag(Key_Task3));
 assert(Input_Level(1,3,1,tick));Input_Release(1);Drive(30);assert(Key_GetFlag(Key_Task3));
 assert(!Input_SetMode(INPUT_REMOTE));gb.value|=GPIO_PIN_12;Drive(150);assert(ui_ok==1);
 assert(Input_SetMode(INPUT_REMOTE));gb.value&=(uint16_t)~GPIO_PIN_13;Drive(50);assert(!Key_GetFlag(Key_Task4));
 gb.value|=GPIO_PIN_13;Drive(30);assert(Input_SetMode(INPUT_PHYSICAL));assert(!Input_Pulse(0,4,100,tick));
 assert(Input_SetMode(INPUT_MIXED));key_primask=1;assert(Input_Level(0,4,1,tick));assert(key_primask==1);key_primask=0;
 Drive(5100);assert(!Input_SourceMask(0,tick)&&!Key_GetFlag(Key_Task4));
 assert(Input_Pulse(0,16,100,tick));Drive(40);assert(Key_GetFlag(Key_Task5));assert(ui_next==1&&ui_ok==1);Drive(120);
 assert(Input_Pulse(0,32,100,tick));Drive(40);assert(Key_GetFlag(Key_Task6));Drive(120);
 gc.value&=(uint16_t)~GPIO_PIN_0;Drive(40);assert(Key_GetFlag(Key_Task5));gc.value|=GPIO_PIN_0;Drive(50);
 gc.value&=(uint16_t)~GPIO_PIN_1;Drive(40);assert(Key_GetFlag(Key_Task6));gc.value|=GPIO_PIN_1;Drive(50);
 assert(Input_Pulse(0,64,100,tick));assert(Input_Resolve(7,0,tick));Drive(150);assert(!Input_Resolve(7,0,tick));
 tick=0xfffffff0U;assert(Input_Pulse(0,4,100,tick));Drive(150);assert(!Key_GetFlag(Key_Task3));
 assert(Input_Pulse(0,4,100,tick)&&Input_Pulse(1,4,900,tick));Input_Release(0);Drive(50);assert(Key_GetFlag(Key_Task3));Input_Release(1);Drive(50);
 assert(!Input_Pulse(2,4,100,tick)&&!Input_Pulse(0,128,100,tick)&&!Input_Pulse(0,1,5001,tick));
 Input_LegacyHeld(1);assert(!Input_SetMode(INPUT_REMOTE));Input_LegacyHeld(0);
 puts("PASS MAPPING: actual BSP+UI, tap/hold/chord, physical OR remote, source isolation, modes, expiry/wrap, PRIMASK");
}
int main(void){
 uint32_t i;uint8_t data[255]={0};const HostDiagnosticsStatus *s;
 assert(BoardUart_InitAll()==BOARD_OK);
 huart2.Init.BaudRate=9600;assert(BoardUart_InitAll()==BOARD_ERROR);huart2.Init.BaudRate=BOARD_UART_SERVO_BAUD;
 huart3.Instance=huart1.Instance;assert(BoardUart_InitAll()==BOARD_ERROR);huart3.Instance=(void*)3;
 assert(BoardResource_GetUart((BoardUartRole)-1)==0);
 tx_result=HAL_TIMEOUT;assert(BoardUart_Transmit(BOARD_UART_SERVO,data,1,5)==BOARD_TIMEOUT);
 assert(FeetechStsPort_Transmit(data,1,5)==HAL_TIMEOUT);
 tx_result=HAL_BUSY;assert(FeetechStsPort_Transmit(data,1,5)==HAL_BUSY);
 tx_result=HAL_OK;assert(FeetechStsPort_Transmit(data,255,0)==HAL_OK);assert(last_timeout==23);
 assert(FeetechStsPort_Transmit(0,1,0)==HAL_ERROR);
 puts("PASS UART: role/baud validation, duplicate resources, invalid role, timeout/busy propagation, wire budget");
 tick=0;BoardLed_Init();assert(gg.value&GPIO_PIN_7);assert(!(gg.value&(GPIO_PIN_0|GPIO_PIN_1)));
 BoardLed_Set(BOARD_LED_HEARTBEAT,BOARD_LED_HEARTBEAT_1HZ);
 BoardLed_Task(0);assert(!(gg.value&GPIO_PIN_7));BoardLed_Task(99);assert(!(gg.value&GPIO_PIN_7));
 BoardLed_Task(100);assert(gg.value&GPIO_PIN_7);BoardLed_Task(1000);assert(!(gg.value&GPIO_PIN_7));
 tick=0xfffffff0U;BoardLed_Set(BOARD_LED_FAULT,BOARD_LED_BLINK_2HZ);
 BoardLed_Task(0x20U);assert(gg.value&GPIO_PIN_1);BoardLed_Task(0x100U);assert(!(gg.value&GPIO_PIN_1));
 BoardLed_Set((BoardLedId)-1,BOARD_LED_ON);
 puts("PASS LED: active-low/high defaults, 100ms duty boundary, uint32 tick rollover, invalid role");
 tick=0;ga.value=gb.value=0xffff;Key_Init();
 ga.value&=(uint16_t)~GPIO_PIN_0;tick=1;Key_Task();tick=10;ga.value|=GPIO_PIN_0;Key_Task();
 tick=20;Key_Task();assert(Key_GetFlag(Key_Task1)==0);assert(Key_TakeEvent(Key_Task1)==0);
 tick=30;ga.value&=(uint16_t)~GPIO_PIN_0;Key_Task();tick=45;Key_Task();assert(Key_GetFlag(Key_Task1)==0);
 tick=46;Key_Task();assert(Key_GetFlag(Key_Task1)==1);assert(Key_TakeEvent(Key_Task1)&KEY_EVENT_PRESS);
 assert(Key_TakeEvent(Key_Task1)==0);
 puts("PASS KEY: bounce rejected, stable press accepted at debounce boundary, event consumed once");
 huart1.RxState=0;HostDiagnostics_Init();tx_len=0;s=HostDiagnostics_GetStatus();
 for(i=0;i<300;i++)feed((uint8_t)i);
 assert(s->rx_bytes==300 && s->rx_overflow==45);
 for(i=0;i<10;i++)HostDiagnostics_Task();
 assert(s->tx_bytes==255 && tx_len==255);for(i=0;i<255;i++)assert(tx_bytes[i]==(uint8_t)i);
 for(i=0;i<100;i++)feed((uint8_t)(255+i));for(i=0;i<4;i++)HostDiagnostics_Task();
 assert(s->tx_bytes==355);for(i=0;i<100;i++)assert(tx_bytes[255+i]==(uint8_t)(255+i));
 feed(0x51);tx_result=HAL_BUSY;HostDiagnostics_Task();assert(s->tx_bytes==355);
 tx_result=HAL_OK;HostDiagnostics_Task();assert(s->tx_bytes==356 && tx_bytes[355]==0x51);
 feed(0x52);tx_result=HAL_TIMEOUT;HostDiagnostics_Task();assert(s->tx_errors==1);
 tx_result=HAL_OK;HostDiagnostics_Task();assert(s->tx_bytes==356);
 huart1.RxState=0;rx_result=HAL_ERROR;HostDiagnostics_Error();assert(s->uart_errors==1 && s->rearm_errors==1);
 rx_result=HAL_OK;HostDiagnostics_Task();feed(0x53);HostDiagnostics_Task();assert(s->tx_bytes==357);
 puts("PASS HOST: bounded overflow, byte order/ring wrap, busy retry, timeout drop, receive recovery");
 MappingTest();
 return 0;
}
'''

def main():
    app = ROOT / "project/App"
    with tempfile.TemporaryDirectory(prefix="h743-p2-tests-") as folder:
        tmp = Path(folder)
        (tmp / "stm32h7xx_hal.h").write_text(HAL, encoding="utf-8")
        (tmp / "test.c").write_text(TEST, encoding="utf-8")
        cmd = [str(GCC), "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation", "-DSTM32H743xx", "-DPLATFORM_TEST_UART_BYTE_ONLY=1"]
        for p in [tmp, app / "config", app / "platform", app / "platform/boards/lxb743zi_p1", app / "modules/ui", app / "bsp", app / "drivers/Servo"]:
            cmd += ["-I", str(p)]
        cmd += [str(tmp / "test.c")] + [str(app / p) for p in [
            "platform/board_resources.c", "platform/board_gpio.c", "platform/board_uart.c", "platform/board_led.c",
            "platform/host_diagnostics.c", "drivers/Servo/feetech_sts_port.c", "bsp/bsp_key.c", "modules/ui/ui_input.c"]]
        cmd += ["-o", str(tmp / "test.exe")]
        compile_result = subprocess.run(cmd, capture_output=True, text=True)
        if compile_result.returncode:
            raise SystemExit(compile_result.stdout + compile_result.stderr)
        run = subprocess.run([str(tmp / "test.exe")], capture_output=True, text=True)
        result = {"scope": "host HAL stub; not hardware validation", "exit_code": run.returncode,
                  "stdout": run.stdout, "stderr": run.stderr, "compiler": str(GCC)}
        out = ROOT / "Docs/适配规划/05_调查证据/P2主机测试.json"
        out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(run.stdout, end="")
        if run.returncode:
            raise SystemExit(run.returncode)

if __name__ == "__main__":
    main()
