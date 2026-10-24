#ifndef BOARD_MOTOR_H
#define BOARD_MOTOR_H
#include "platform_hal.h"
HAL_StatusTypeDef BoardMotor_Init(void);
void BoardMotor_Write(int16_t permille);
uint16_t BoardMotor_Count(void);
uint8_t BoardMotor_Knob(void);
uint8_t BoardMotor_Button(void);
uint8_t BoardMotor_DriverAvailable(void);
void BoardMotor_Irq(void);
#endif
