/*********************************************************************************************************************
 * @file    claw_host_protocol.c
 * @brief   飞特外形的夹爪上位机命令协议实现。
 *********************************************************************************************************************/

#include "claw_host_protocol.h"

// 将命令和浮点值组装为统一 F5 帧。
uint8_t ClawHostProtocol_Build(uint8_t command, float value, uint8_t *frame, uint8_t frame_size)
{
    union {
        uint32_t raw;
        float value;
    } converter;

    if ((frame == 0) || (frame_size < CLAW_HOST_FRAME_TOTAL_LEN) ||
        (command > CLAW_HOST_CMD_MAX)) {
        return 0U;
    }
    converter.value = value;
    if ((converter.raw & 0x7F800000UL) == 0x7F800000UL) {
        return 0U;
    }

    frame[0] = 0xFFU;
    frame[1] = 0xFFU;
    frame[2] = CLAW_HOST_FRAME_ID;
    frame[3] = CLAW_HOST_FRAME_LENGTH_FIELD;
    frame[4] = CLAW_HOST_INST_ACTION;
    frame[5] = command;
    frame[6] = (uint8_t)(converter.raw & 0xFFUL);
    frame[7] = (uint8_t)((converter.raw >> 8) & 0xFFUL);
    frame[8] = (uint8_t)((converter.raw >> 16) & 0xFFUL);
    frame[9] = (uint8_t)((converter.raw >> 24) & 0xFFUL);
    frame[10] = 0x00U;
    return CLAW_HOST_FRAME_TOTAL_LEN;
}
// 判断飞特外形的本地 F5 帧；末字节允许固定 00，便于 VOFA 的 %% 参数模板。
uint8_t ClawHostProtocol_IsFrame(const uint8_t *frame, uint8_t len)
{
    if ((frame == 0) || (len != CLAW_HOST_FRAME_TOTAL_LEN)) {
        return 0U;
    }
    if ((frame[0] != 0xFFU) || (frame[1] != 0xFFU) ||
        (frame[2] != CLAW_HOST_FRAME_ID) ||
        (frame[3] != CLAW_HOST_FRAME_LENGTH_FIELD) ||
        (frame[4] != CLAW_HOST_INST_ACTION) ||
        (frame[5] > CLAW_HOST_CMD_MAX)) {
        return 0U;
    }
    return 1U;
}

// 解析 F5 命令和 4 字节小端浮点参数。
uint8_t ClawHostProtocol_Decode(const uint8_t *frame, uint8_t len, ClawHostCommand_t *out_command)
{
    union {
        uint32_t raw;
        float value;
    } converter;

    if ((out_command == 0) || (ClawHostProtocol_IsFrame(frame, len) == 0U)) {
        return 0U;
    }

    converter.raw = (uint32_t)frame[6] |
                    ((uint32_t)frame[7] << 8) |
                    ((uint32_t)frame[8] << 16) |
                    ((uint32_t)frame[9] << 24);
    if ((converter.raw & 0x7F800000UL) == 0x7F800000UL) {
        return 0U;
    }

    out_command->command = frame[5];
    out_command->value = converter.value;
    return 1U;
}
