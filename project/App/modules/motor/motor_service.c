#include "motor_service.h"
#include "input_control.h"
#include "input_config.h"
#include "motor_config.h"
#include "board_motor.h"
#include "vendor_log.h"
#include <string.h>
static MotorStatus s;
static uint8_t s_maintenance;
static MotorKnob s_knob;
static uint16_t s_last_count, s_key_age;
static uint8_t s_phase, s_key_candidate, s_key_stable, s_debounce, s_long;
static uint32_t s_last_tick;
static uint32_t Lock(void) {uint32_t p=__get_PRIMASK();__disable_irq();return p;}
static void Unlock(uint32_t p) {__set_PRIMASK(p);}
static uint8_t EnableInternal(uint8_t on)
{
    float target=s.control.target_rpm;
    uint8_t ok;
    if(on && (!s.ready || !s.driver || s_maintenance)) {s.blocked_starts++;return 0;}
    ok=(uint8_t)MotorControl_Enable(&s.control,on);
    /* Normal toggle OFF retains the knob setting. Explicit STOP/fault clears it. */
    if(!on)s.control.target_rpm=target;
    return ok;
}
void MotorService_Init(void)
{
    MotorConfig cfg={MOTOR_KP,MOTOR_KI,MOTOR_KD,MOTOR_ENCODER_COUNTS_PER_REV,
                     MOTOR_MAX_RPM,MOTOR_RAMP_RPM_S,MOTOR_PWM_LIMIT,MOTOR_SPEED_FILTER};
    memset(&s,0,sizeof(s));MotorControl_Init(&s.control,&cfg);
    (void)MotorControl_SetTarget(&s.control,MOTOR_DEFAULT_RPM);
    s_phase=0;s_key_age=0;s_key_candidate=s_key_stable=s_debounce=s_long=0;
    s_maintenance=0;
    s.driver=BoardMotor_DriverAvailable();
    if(BoardMotor_Init()!=HAL_OK)return;
    s_last_count=BoardMotor_Count();s_knob.previous=BoardMotor_Knob();s_knob.quarters=0;s_knob.edges=MOTOR_KNOB_EDGES_PER_DETENT;
    s_last_tick=HAL_GetTick();s.ready=1;
}
uint8_t MotorService_IsRunning(void) {uint32_t p=Lock();uint8_t enabled=s.control.enabled;Unlock(p);return enabled;}
uint8_t MotorService_Enable(uint8_t on)
{uint32_t p=Lock();uint8_t ok=EnableInternal(on);if(!on)BoardMotor_Write(0);Unlock(p);return ok;}
uint8_t MotorService_SetTarget(uint16_t rpm)
{uint32_t p=Lock();uint8_t ok=(uint8_t)MotorControl_SetTarget(&s.control,(float)rpm);Unlock(p);return ok;}
void MotorService_Reverse(void) {uint32_t p=Lock();MotorControl_Reverse(&s.control);Unlock(p);}
void MotorService_Stop(void) {uint32_t p=Lock();MotorControl_Stop(&s.control);BoardMotor_Write(0);Unlock(p);}
void MotorService_ClearFault(void) {uint32_t p=Lock();MotorControl_ClearFault(&s.control);BoardMotor_Write(0);Unlock(p);}
uint8_t MotorService_SetCpr(uint32_t cpr)
{uint32_t p; if(!cpr || cpr>1000000U)return 0;p=Lock();if(s.control.enabled){Unlock(p);return 0;}s.control.config.counts_per_rev=(float)cpr;Unlock(p);return 1;}
uint8_t MotorService_SetPid(uint32_t kp,uint32_t ki,uint32_t kd)
{uint32_t p;if(kp>100000U || ki>100000U || kd>100000U)return 0;p=Lock();if(s.control.enabled){Unlock(p);return 0;}s.control.config.kp=kp/1000.0f;s.control.config.ki=ki/1000.0f;s.control.config.kd=kd/1000.0f;Unlock(p);return 1;}
uint8_t MotorService_ZeroCount(void)
{uint32_t p=Lock();if(!s.ready || s.control.enabled){Unlock(p);return 0;}s.control.total_counts=0;s_last_count=BoardMotor_Count();Unlock(p);return 1;}
void MotorService_Snapshot(MotorStatus *out) {uint32_t p=Lock();*out=s;Unlock(p);}
void MotorService_Report(void)
{
    MotorStatus v;MotorService_Snapshot(&v);
    VendorLog("MOTOR ready=%u driver=%u on=%u fault=%u dir=%d target=%ld rpm10=%ld pwm=%d cpr1000=%lu count=%lld blocked=%lu invalid=%lu\r\n",
        v.ready,v.driver,v.control.enabled,(unsigned)v.control.fault,v.control.requested_direction,
        (long)v.control.target_rpm,(long)(v.control.measured_rpm*10),v.control.pwm,
        (unsigned long)(v.control.config.counts_per_rev*1000.0f+0.5f),(long long)v.control.total_counts,
        (unsigned long)v.blocked_starts,(unsigned long)v.knob_invalid);
}
/* Both EC11 detents and host detents use the same bounded target update. */
static void ApplyDetents(int16_t steps){
 float target=s.control.target_rpm+steps*MOTOR_KNOB_STEP_RPM;
 if(target<0)target=0;if(target>s.control.config.max_rpm)target=s.control.config.max_rpm;
 (void)MotorControl_SetTarget(&s.control,target);
}
uint8_t MotorService_SimulateKnob(int16_t steps){
 uint32_t p;
 if(!steps||steps < -80||steps > 80||Input_GetMode()==INPUT_PHYSICAL)return 0;
 p=Lock();if(!s.ready||s_maintenance||!Input_DebugEnabled(1)){Unlock(p);return 0;}ApplyDetents(steps);Unlock(p);return 1;
}
void MotorService_Tick1ms(void)
{
    uint8_t pressed,ab;int8_t detent;uint16_t count;uint32_t now;
    if(!s.ready)return;
    now=HAL_GetTick();
    if((uint32_t)(now-s_last_tick)>50U && s.control.enabled) {
        MotorControl_Stop(&s.control);s.control.fault=MOTOR_FAULT_TIMING;BoardMotor_Write(0);
    }
    s_last_tick=now;
    ab=BoardMotor_Knob();
    if(Input_GetMode()==INPUT_REMOTE){s_knob.previous=ab;s_knob.quarters=0;detent=0;}
    else {if((ab^s_knob.previous)==3U)s.knob_invalid++;detent=MotorKnob_Sample(&s_knob,ab);}
    if(detent && Input_DebugEnabled(1) && Input_GetMode()!=INPUT_REMOTE)ApplyDetents(detent);
    pressed=Input_Resolve(INPUT_MOTOR_KEY,BoardMotor_Button(),now);
    if(!Input_DebugEnabled(1)){
        s_key_candidate=s_key_stable=s_debounce=s_long=0;s_key_age=0;pressed=0;
    }
    if(pressed!=s_key_candidate){s_key_candidate=pressed;s_debounce=0;}
    else if(s_debounce<INPUT_MOTOR_DEBOUNCE_MS)s_debounce++;
    if(s_debounce==INPUT_MOTOR_DEBOUNCE_MS && s_key_stable!=pressed) {
        s_key_stable=pressed;
        if(pressed){s_key_age=0;s_long=0;}
        else if(!s_long){(void)EnableInternal(!s.control.enabled);if(!s.control.enabled)BoardMotor_Write(0);}
    }
    if(s_key_stable && s_key_age<MOTOR_KEY_LONG_MS) {
        s_key_age++;
        if(s_key_age==MOTOR_KEY_LONG_MS){s_long=1;MotorControl_Reverse(&s.control);}
    }
    if(++s_phase<10U)return;s_phase=0;
    count=BoardMotor_Count();
    MotorControl_Step(&s.control,MotorControl_Delta16(count,s_last_count)*MOTOR_ENCODER_SIGN);
    s_last_count=count;BoardMotor_Write(s.control.pwm);
}

/* Restore RAM parameters only; never enable output, erase Flash or clear a fault. */
uint8_t MotorService_ResetParameters(void)
{
 uint32_t p=Lock();
 if(!s.ready || s.control.enabled){Unlock(p);return 0;}
 MotorControl_Stop(&s.control);
 s.control.config.kp=MOTOR_KP;s.control.config.ki=MOTOR_KI;s.control.config.kd=MOTOR_KD;
 s.control.config.counts_per_rev=MOTOR_ENCODER_COUNTS_PER_REV;
 s.control.config.max_rpm=MOTOR_MAX_RPM;s.control.config.ramp_rpm_s=MOTOR_RAMP_RPM_S;
 s.control.config.max_pwm=MOTOR_PWM_LIMIT;s.control.config.filter_alpha=MOTOR_SPEED_FILTER;
 (void)MotorControl_SetTarget(&s.control,MOTOR_DEFAULT_RPM);
 BoardMotor_Write(0);Unlock(p);return 1;
}

uint8_t MotorService_BeginMaintenance(void){uint32_t p=Lock();if(!s.ready||s.control.enabled||s_maintenance){Unlock(p);return 0;}s_maintenance=1;BoardMotor_Write(0);Unlock(p);return 1;}
void MotorService_EndMaintenance(void){uint32_t p=Lock();s_maintenance=0;Unlock(p);}
uint8_t MotorService_ApplyParameters(float cpr,uint32_t rpm,uint32_t kp,uint32_t ki,uint32_t kd){
 uint32_t p;
 if(!(cpr>0 && cpr<=1000000.0f)||rpm>(uint32_t)MOTOR_MAX_RPM||kp>100000U||ki>100000U||kd>100000U)return 0;
 p=Lock();if(!s.ready||s.control.enabled){Unlock(p);return 0;}
 MotorControl_Stop(&s.control);s.control.config.counts_per_rev=cpr;s.control.config.kp=kp/1000.0f;s.control.config.ki=ki/1000.0f;s.control.config.kd=kd/1000.0f;
 (void)MotorControl_SetTarget(&s.control,(float)rpm);BoardMotor_Write(0);Unlock(p);return 1;
}
