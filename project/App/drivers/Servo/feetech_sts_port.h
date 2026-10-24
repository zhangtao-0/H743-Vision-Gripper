
#ifndef FEETECH_STS_PORT_H
#define FEETECH_STS_PORT_H

#include "platform_hal.h"
#include <stdint.h>

HAL_StatusTypeDef FeetechStsPort_StartReceiveIT(uint8_t *data);

HAL_StatusTypeDef FeetechStsPort_Transmit(
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms);

void FeetechStsPort_ClearOverrun(void);

uint8_t FeetechStsPort_IsUart(
    const UART_HandleTypeDef *huart);

#endif
