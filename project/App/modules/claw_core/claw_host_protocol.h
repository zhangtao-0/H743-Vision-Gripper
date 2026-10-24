/*********************************************************************************************************************
 * @file    claw_host_protocol.h
 * @brief   飞特外形的夹爪上位机命令协议。
 *********************************************************************************************************************/

#ifndef __CLAW_HOST_PROTOCOL_H__
#define __CLAW_HOST_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CLAW_HOST_INST_ACTION                 0xF5U
#define CLAW_HOST_FRAME_ID                    0xFEU
#define CLAW_HOST_FRAME_LENGTH_FIELD          7U
#define CLAW_HOST_FRAME_TOTAL_LEN             11U

#define CLAW_HOST_CMD_ANGLE                   0U  // 目标角度：float，单位度
#define CLAW_HOST_CMD_A0                      1U  // A0：1单击，2双击，3停止
#define CLAW_HOST_CMD_B0                      2U  // B0：0停止，1扫描，2反向
#define CLAW_HOST_CMD_RANGE                   3U  // 范围：0机械极限，1为0~270度
#define CLAW_HOST_CMD_PWM1_KEY                4U  // PWM1虚拟按键：0松开，1按下
#define CLAW_HOST_CMD_PWM2_KEY                5U  // PWM2虚拟按键：0松开，1按下
#define CLAW_HOST_CMD_MAX                     CLAW_HOST_CMD_PWM2_KEY

typedef struct {
    uint8_t command;
    float value;
} ClawHostCommand_t;

uint8_t ClawHostProtocol_Build(uint8_t command, float value, uint8_t *frame, uint8_t frame_size); // 命令和值 -> F5 帧。
uint8_t ClawHostProtocol_IsFrame(const uint8_t *frame, uint8_t len); // 判断是否为 F5 上位机帧。
uint8_t ClawHostProtocol_Decode(const uint8_t *frame, uint8_t len, ClawHostCommand_t *out_command); // F5 Params -> 命令和值。

#ifdef __cplusplus
}
#endif

#endif
