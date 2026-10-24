#ifndef MOTOR_PLOT_H
#define MOTOR_PLOT_H
#include <stdint.h>
void MotorPlot_Init(void);
uint8_t MotorPlot_Enable(uint8_t on);
uint8_t MotorPlot_IsEnabled(void);
void MotorPlot_Task(uint32_t now);
void MotorPlot_Report(void);
#endif
