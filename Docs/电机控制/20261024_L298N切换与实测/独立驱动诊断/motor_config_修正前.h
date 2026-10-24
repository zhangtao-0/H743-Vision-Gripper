#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H
/* 0=unfitted, 1=PWM+IN1+IN2, 2=PWM+IN1+IN2+STBY,
 * 3=two-input PWM H-bridge. Select only after checking its truth table. */
#ifndef MOTOR_DRIVER_PROFILE
#define MOTOR_DRIVER_PROFILE 1 /* L298N ENA+IN1+IN2; VSS=5V, 3.3V control signals */
#endif
#if MOTOR_DRIVER_PROFILE < 0 || MOTOR_DRIVER_PROFILE > 3
#error "Invalid motor H-bridge profile"
#endif
/* User-provided MG310 specification: actual gear ratio 20.409,
 * Hall=13PPR; alternative GMR=500PPR. TIM1 TI12 counts all four edges. */
#ifndef MOTOR_ENCODER_GMR
#define MOTOR_ENCODER_GMR 0
#endif
#define MOTOR_GEAR_RATIO 20.409f
#define MOTOR_ENCODER_PPR (MOTOR_ENCODER_GMR ? 500.0f : 13.0f)
#define MOTOR_ENCODER_COUNTS_PER_REV (4.0f * MOTOR_ENCODER_PPR * MOTOR_GEAR_RATIO)
#define MOTOR_ENCODER_SIGN 1
#define MOTOR_OUTPUT_SIGN 1
#define MOTOR_MAX_RPM 400.0f /* application limit, not a claimed MG310 rating */
#define MOTOR_DEFAULT_RPM 400.0f /* setpoint only; startup remains disabled */
#define MOTOR_KNOB_EDGES_PER_DETENT 4U /* some EC11 variants use 2 */
#if MOTOR_KNOB_EDGES_PER_DETENT != 2 && MOTOR_KNOB_EDGES_PER_DETENT != 4
#error "EC11 detent must contain 2 or 4 edges"
#endif
#define MOTOR_KNOB_STEP_RPM 5.0f
#define MOTOR_KEY_LONG_MS 1000U /* short release toggles enable; hold reverses */
#define MOTOR_KP 1.8f /* permille/RPM; initial tuning values, bench tuning required */
#define MOTOR_KI 5.0f
#define MOTOR_KD 0.0f
#define MOTOR_PWM_LIMIT 950.0f
#define MOTOR_RAMP_RPM_S 150.0f
#define MOTOR_SPEED_FILTER 0.25f
#endif
