"""P3 read-only servo state machine + STS protocol host test (HAL stub, no board)."""
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
typedef enum {HAL_OK=0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef struct {void *Instance; struct {uint32_t BaudRate;} Init; uint32_t RxState;} UART_HandleTypeDef;
#define HAL_UART_STATE_READY 0U
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
#define __HAL_UART_CLEAR_OREFLAG(u) ((void)(u))
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *,const uint8_t *,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
#endif
'''

TEST = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "feetech_sts_bus.h"
#include "servo_p3.h"

UART_HandleTypeDef huart1={(void*)1,{BOARD_UART_HOST_BAUD},0};
UART_HandleTypeDef huart2={(void*)2,{BOARD_UART_SERVO_BAUD},0};
UART_HandleTypeDef huart3={(void*)3,{BOARD_UART_BLUETOOTH_BAUD},0};

static uint32_t tick;
static uint8_t *rx_ptr;
static uint8_t tx[4096];
static uint32_t tx_len;
static char telemetry[4096];
static uint32_t telemetry_len;

uint32_t HAL_GetTick(void){return tick;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef*u,uint8_t*p,uint16_t n){(void)u;(void)p;(void)n;return HAL_ERROR;}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u,const uint8_t *b,uint16_t n,uint32_t timeout){
    (void)timeout;
    if(u!=&huart2) return HAL_OK;
    assert(tx_len+n<=sizeof(tx));
    memcpy(tx+tx_len,b,n);
    tx_len+=n;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u){u->RxState=HAL_UART_STATE_READY;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *u){assert(u->RxState==HAL_UART_STATE_READY);return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n){
    assert(n==1U);
    if(u==&huart2) rx_ptr=p;
    u->RxState=1U;
    return HAL_OK;
}

/* servo_p3.c only needs this VOFA entry point in the host test. */
HAL_StatusTypeDef VOFA_Protocol_SendRaw(const uint8_t *data,uint16_t len){
    assert(telemetry_len+len<sizeof(telemetry));
    memcpy(telemetry+telemetry_len,data,len);
    telemetry_len+=len;
    return HAL_OK;
}

static void feed_byte(uint8_t b){
    assert(rx_ptr);
    *rx_ptr=b;
    huart2.RxState=HAL_UART_STATE_READY;
    FeetechStsBus_HandleRxComplete();
}

static uint8_t reply_error;
static void feed_resp(uint8_t id,const uint8_t *params,uint8_t n){
    uint8_t f[80];
    uint8_t i,total;
    f[0]=0xFF; f[1]=0xFF; f[2]=id; f[3]=(uint8_t)(n+2U); f[4]=reply_error;
    for(i=0;i<n;i++) f[5+i]=params[i];
    total=(uint8_t)(6U+n);
    f[total-1]=FeetechSts_CalcChecksum(f,total);
    for(i=0;i<total;i++) feed_byte(f[i]);
}

static void assert_all_tx_read_only(void){
    uint32_t i=0;
    while(i+5U<tx_len){
        assert(tx[i]==0xFF && tx[i+1]==0xFF);
        {
            uint8_t len=tx[i+3];
            uint8_t inst=tx[i+4];
            assert(inst==FEETECH_STS_INST_PING || inst==FEETECH_STS_INST_READ);
            i += (uint32_t)len + 4U;
        }
    }
}

int main(void){
    uint8_t p[16];
    const ServoP3Status *s;
    uint8_t frame[80];
    uint8_t n;

    /* Pure protocol boundaries. */
    n=FeetechSts_BuildPacket(1,FEETECH_STS_INST_PING,0,0,frame,sizeof(frame));
    assert(n==6U);
    assert(frame[0]==0xFF && frame[1]==0xFF && frame[2]==1U && frame[3]==2U && frame[4]==FEETECH_STS_INST_PING);
    assert(FeetechSts_VerifyChecksum(frame,n)==1U);
    assert(FeetechSts_AngleToPosition(270.0f)==3072U);
    assert(FeetechSts_AngleToPosition(0.0f)==0U);
    assert(FeetechSts_LimitAngle(300.0f)==270.0f);
    assert(FeetechSts_LimitAngle(-10.0f)==0.0f);
    assert(FeetechSts_PositionToAngle(2048U)>179.9f && FeetechSts_PositionToAngle(2048U)<180.1f);
    puts("PASS PROTO: frame build/checksum, angle/position, limit");

    tick=0;
    ServoP3_Init();
    s=ServoP3_GetStatus();
    assert(s->state==SERVO_P3_STATE_PING);

    ServoP3_Task(0);
    assert(tx_len==6U); /* PING */
    assert(tx[4]==FEETECH_STS_INST_PING);

    feed_resp(BOARD_SERVO_ID,0,0);
    ServoP3_Task(1);
    assert(s->state==SERVO_P3_STATE_READ_ID);
    assert(tx_len==14U);
    assert(tx[tx_len-4]==FEETECH_STS_INST_READ);

    p[0]=BOARD_SERVO_ID;
    feed_resp(BOARD_SERVO_ID,p,1U); /* configured ID */
    ServoP3_Task(2);
    assert(s->reported_id==BOARD_SERVO_ID);
    assert(s->state==SERVO_P3_STATE_READ_MODEL);

    p[0]=0x34U; p[1]=0x12U;
    feed_resp(BOARD_SERVO_ID,p,2U); /* model = 0x1234 */
    ServoP3_Task(3);
    assert(s->model==0x1234U);
    assert(s->state==SERVO_P3_STATE_READ_BAUD);

    p[0]=0U;
    feed_resp(BOARD_SERVO_ID,p,1U); /* baud index 0 */
    ServoP3_Task(4);
    assert(s->baud_index==0U);
    assert(s->state==SERVO_P3_STATE_READ_LIMITS);

    p[0]=0x00U; p[1]=0x00U; p[2]=0xFFU; p[3]=0x0FU; /* 0..4095 */
    feed_resp(BOARD_SERVO_ID,p,4U);
    ServoP3_Task(5);
    assert(s->state==SERVO_P3_STATE_ONLINE);
    assert(s->min_position==0U && s->max_position==4095U);

    ServoP3_Task(6); /* enter ONLINE -> send READ_FEEDBACK */
    /* Feedback: 0x38..0x46, 15 bytes. */
    memset(p,0,sizeof(p));
    p[0]=0x00U; p[1]=0x08U;   /* position 2048 -> 180.0 deg */
    p[6]=120U;                 /* 12.0 V */
    p[7]=30U;                  /* 30 C */
    p[10]=0U;                  /* moving */
    feed_resp(BOARD_SERVO_ID,p,15U);
    tick=100U;
    ServoP3_Task(100U);
    assert(s->online==1U);
    assert(s->position==2048U);
    assert(s->angle_tenths==1800);
    assert(s->voltage_mv==12000U);
    assert(s->temperature_c==30U);
    tick=600U;
    ServoP3_Task(600U); /* telemetry period reached */
    assert(telemetry[0]==0); /* default silence while probing and online */
    ServoP3_SetTelemetry(1);tick=601U;ServoP3_Task(601U);
    assert(strstr(telemetry,"P3 st=ONLINE")!=0);
    assert(strstr(telemetry,"model=0x1234")!=0);

    assert_all_tx_read_only();

    /* Offline: no response -> timeout -> RETRY, still no writes. */
    tick=1200U;
    ServoP3_Task(1200U);
    tick=1301U;
    ServoP3_Task(1301U);
    assert(s->online==0U);
    assert(s->state==SERVO_P3_STATE_RETRY);
    assert_all_tx_read_only();

    /* Re-init and verify baud fallback: 3 failed pings at 1 Mbps -> 9600. */
    tick=2000U;
    ServoP3_Init();
    ServoP3_Task(2000U);          /* PING @1M */
    tick=2101U; ServoP3_Task(2101U); /* timeout 1 */
    tick=2401U; ServoP3_Task(2401U); /* PING */
    tick=2502U; ServoP3_Task(2502U); /* timeout 2 */
    tick=2802U; ServoP3_Task(2802U); /* PING */
    tick=3103U; ServoP3_Task(3103U); /* timeout 3 -> switch 9600 */
    assert(huart2.Init.BaudRate==9600U);
    assert(s->mcu_baud==9600U);
    assert(s->baud_step==1U);

    tick=3104U; ServoP3_Task(3104U); /* PING @9600 */
    feed_resp(BOARD_SERVO_ID,0,0);
    ServoP3_Task(3105U);
    assert(s->state==SERVO_P3_STATE_READ_ID);
    assert(s->baud_locked==1U);
    assert_all_tx_read_only();

    assert(FeetechStsBus_RawRxBytes()>0U);
    tick=4000; ServoP3_Init();
    assert(ServoP3_ProbeAt(254U,1000000U)==0U);
    assert(ServoP3_ProbeAt(1U,9800U)==1U); /* legal MCU rate, not an asserted servo default */
    assert(ServoP3_ProbeAt(1U,1000000U)==1U);
    ServoP3_Task(4000);feed_resp(1,0,0);ServoP3_Task(4001);
    assert(s->state==SERVO_P3_STATE_READ_ID && s->reported_id==1 && s->baud_locked);
    assert_all_tx_read_only();
    tick=5000;ServoP3_Init();ServoP3_SetTelemetry(0);tx_len=0;assert(ServoP3_StartScan(1000000));
    for(unsigned id=0;id<=253;id++){ServoP3_Task(tick);assert(tx[tx_len-4]==id);tick+=101;ServoP3_Task(tick);}
    assert(!s->scan_active&&s->state==SERVO_P3_STATE_IDLE);assert_all_tx_read_only();
    tick+=100;ServoP3_Init();assert(ServoP3_StartScan(1000000));ServoP3_Task(tick);tick+=101;ServoP3_Task(tick);ServoP3_Task(++tick);
    assert(s->reported_id==1);feed_resp(1,0,0);ServoP3_Task(++tick);assert(s->state==SERVO_P3_STATE_READ_ID&&s->scan_active);
    p[0]=0;feed_resp(1,p,1);ServoP3_Task(++tick);assert(s->state==SERVO_P3_STATE_RETRY);
    assert(BOARD_SERVO_ID==1&&FEETECH_STS_DEFAULT_ID==BOARD_SERVO_ID&&FEETECH_STS_BROADCAST_ID==254);
    assert_all_tx_read_only();puts("PASS SCAN: unicast 0..253, stops after exhaustion, identity mismatch rejected; no writes");

    /* ERROR=1 PING is identical to TX; retain only as candidate, READ proves identity. */
    tx_len=0;tick+=1000;ServoP3_Init();ServoP3_Task(tick);reply_error=1;
    feed_resp(1,0,0);ServoP3_Task(++tick);
    assert(s->state==SERVO_P3_STATE_READ_ID&&!s->online&&!FeetechStsBus_GetFeedback()->online);
    p[0]=1;feed_resp(1,p,1);ServoP3_Task(++tick);assert(s->state==SERVO_P3_STATE_READ_MODEL);
    p[0]=0x34;p[1]=0x12;feed_resp(1,p,2);ServoP3_Task(++tick);
    p[0]=0;feed_resp(1,p,1);ServoP3_Task(++tick);
    p[0]=0;p[1]=0;p[2]=0xff;p[3]=0x0f;feed_resp(1,p,4);ServoP3_Task(++tick);
    assert(s->state==SERVO_P3_STATE_ONLINE&&s->online&&s->last_error==1&&s->model==0x1234);
    assert(FeetechStsBus_GetFeedback()->error==1);assert_all_tx_read_only();
    /* Echo only cannot pass the identity read or terminate scan at a phantom ID. */
    tick+=1000;ServoP3_Init();tx_len=0;assert(ServoP3_StartScan(1000000));ServoP3_Task(tick);
    feed_resp(0,0,0);ServoP3_Task(++tick);assert(s->state==SERVO_P3_STATE_READ_ID&&!s->online&&s->scan_active);
    unsigned begin=tx_len-8;for(unsigned j=begin;j<begin+8;j++)feed_byte(tx[j]); /* READ echo */
    tick+=101;ServoP3_Task(tick);assert(s->state==SERVO_P3_STATE_PING&&s->reported_id==1&&s->scan_active&&!s->online);
    reply_error=0;assert_all_tx_read_only();puts("PASS alarm/echo: voltage-error reply preserved, identity verified, echo-only cannot establish online");
    puts("PASS P3: ping/id/model/baud/limits/feedback, offline retry, safe baud rearm, explicit ID probe, no writes");
    return 0;
}
'''

def main():
    app = ROOT / "project/App"
    with tempfile.TemporaryDirectory(prefix="h743-p3-tests-") as folder:
        tmp = Path(folder)
        (tmp / "stm32h7xx_hal.h").write_text(HAL, encoding="utf-8")
        (tmp / "test.c").write_text(TEST, encoding="utf-8")
        cmd = [str(GCC), "-std=c99", "-Wall", "-Wextra", "-Werror",
               "-Wno-misleading-indentation", "-DSTM32H743xx", "-DPLATFORM_TEST_UART_BYTE_ONLY=1", "-O0"]
        for p in [tmp, app / "config", app / "platform", app / "platform/boards/lxb743zi_p1",
                  app / "config", app / "drivers/Servo", app / "modules/servo_p3",
                  app / "protocol", app / "modules/claw_core"]:
            cmd += ["-I", str(p)]
        cmd += [str(tmp / "test.c")] + [str(app / p) for p in [
            "platform/board_resources.c", "platform/board_uart.c",
            "drivers/Servo/feetech_sts_port.c", "drivers/Servo/feetech_sts_bus.c",
            "modules/servo_p3/servo_p3.c"]]
        cmd += ["-o", str(tmp / "test.exe"), "-lm"]
        compile_result = subprocess.run(cmd, capture_output=True, text=True)
        if compile_result.returncode:
            raise SystemExit(compile_result.stdout + compile_result.stderr)
        run = subprocess.run([str(tmp / "test.exe")], capture_output=True, text=True)
        result = {"scope": "host HAL stub; not hardware validation",
                  "exit_code": run.returncode, "stdout": run.stdout,
                  "stderr": run.stderr, "compiler": str(GCC)}
        out = ROOT / "Docs/适配规划/05_调查证据/P3主机测试.json"
        out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(run.stdout, end="")
        if run.returncode:
            raise SystemExit(run.returncode)

if __name__ == "__main__":
    main()
