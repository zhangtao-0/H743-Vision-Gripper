#ifndef BOARD_PERIPHERALS_H
#define BOARD_PERIPHERALS_H
#include "platform_hal.h"
void BoardMemory_Configure(void);
void BoardStartupGpio_Init(void);
HAL_StatusTypeDef BoardPeripherals_ClockInit(void);
HAL_StatusTypeDef BoardRtc_Init(void);
HAL_StatusTypeDef BoardRtc_Set(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
HAL_StatusTypeDef BoardRtc_Get(RTC_TimeTypeDef *time, RTC_DateTypeDef *date);
HAL_StatusTypeDef BoardPwm_SetUs(uint16_t pulse_us);
HAL_StatusTypeDef BoardPwm_SetChannelUs(uint8_t channel, uint16_t pulse_us);
void BoardPwm_Stop(void);
#endif
