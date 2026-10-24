#ifndef MOTOR_SERVICE_H
#define MOTOR_SERVICE_H
#include "motor_control.h"
typedef struct {MotorControl control;uint8_t ready,driver;uint32_t blocked_starts,knob_invalid;} MotorStatus;
void MotorService_Init(void);
void MotorService_Tick1ms(void);
uint8_t MotorService_SimulateKnob(int16_t steps);
uint8_t MotorService_IsRunning(void);
uint8_t MotorService_Enable(uint8_t);
uint8_t MotorService_SetTarget(uint16_t rpm);
void MotorService_Reverse(void);
void MotorService_Stop(void);
void MotorService_ClearFault(void);
uint8_t MotorService_SetCpr(uint32_t);
uint8_t MotorService_SetPid(uint32_t kp_milli,uint32_t ki_milli,uint32_t kd_milli);
uint8_t MotorService_ZeroCount(void);
void MotorService_Snapshot(MotorStatus *);
void MotorService_Report(void);
uint8_t MotorService_ResetParameters(void);
uint8_t MotorService_BeginMaintenance(void);
void MotorService_EndMaintenance(void);
uint8_t MotorService_ApplyParameters(float cpr,uint32_t rpm,uint32_t kp,uint32_t ki,uint32_t kd);
#endif
