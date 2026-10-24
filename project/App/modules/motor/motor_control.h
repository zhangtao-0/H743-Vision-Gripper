#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H
#include <stdint.h>
typedef enum {MOTOR_FAULT_NONE, MOTOR_FAULT_NO_FEEDBACK, MOTOR_FAULT_REVERSE_TIMEOUT, MOTOR_FAULT_FEEDBACK_SIGN, MOTOR_FAULT_TIMING, MOTOR_FAULT_OVERSPEED} MotorFault;
typedef struct {
    float kp, ki, kd, counts_per_rev, max_rpm, ramp_rpm_s, max_pwm;
    float filter_alpha;
} MotorConfig;
typedef struct {
    MotorConfig config;
    float target_rpm, measured_rpm, raw_rpm, ramped_rpm, integral, previous_rpm;
    int16_t pwm;
    int8_t requested_direction, active_direction;
    uint8_t enabled, reversing;
    MotorFault fault;
    uint16_t quiet_ms, reverse_ms, no_feedback_ms, wrong_sign_ms, overspeed_ms;
    int64_t total_counts;
    int64_t feedback_progress; /* signed counts in commanded direction over a300ms window */
} MotorControl;
void MotorControl_Init(MotorControl *, const MotorConfig *);
int MotorControl_Enable(MotorControl *, uint8_t);
int MotorControl_SetTarget(MotorControl *, float);
void MotorControl_Reverse(MotorControl *);
void MotorControl_Stop(MotorControl *);
void MotorControl_ClearFault(MotorControl *);
/* Exactly 10ms, called from a dedicated hardware-timer ISR. */
void MotorControl_Step(MotorControl *, int32_t delta);
/* Gray-code decoder: 4 legal quarter transitions per detent by default. */
typedef struct {uint8_t previous; int8_t quarters;uint8_t edges;} MotorKnob;
int8_t MotorKnob_Sample(MotorKnob *, uint8_t ab);
int32_t MotorControl_Delta16(uint16_t current, uint16_t previous);
#endif
