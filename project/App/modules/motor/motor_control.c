#include "motor_control.h"
#include <string.h>
#define DT 0.01f
static float Abs(float x) { return x < 0 ? -x : x; }
static float Clamp(float x, float lo, float hi) {return x < lo ? lo : (x > hi ? hi : x);}
static void ResetPid(MotorControl *m) {m->integral=0; m->ramped_rpm=0; m->pwm=0; m->previous_rpm=m->measured_rpm*m->active_direction; m->no_feedback_ms=0; m->feedback_progress=0; m->wrong_sign_ms=0;m->overspeed_ms=0;}
static void Fail(MotorControl *m, MotorFault fault) {m->fault=fault; m->enabled=0; ResetPid(m);}
void MotorControl_Init(MotorControl *m, const MotorConfig *cfg)
{memset(m,0,sizeof(*m));m->config=*cfg;m->requested_direction=1;m->active_direction=1;}
void MotorControl_Stop(MotorControl *m)
{m->enabled=0;m->target_rpm=0;m->reversing=0;m->reverse_ms=0;m->quiet_ms=0;ResetPid(m);}
int MotorControl_Enable(MotorControl *m, uint8_t enable)
{
    if (!enable) {MotorControl_Stop(m); return 1;}
    if (m->fault || !(m->config.counts_per_rev>0) || !(m->config.max_rpm>0)) return 0;
    ResetPid(m);m->enabled=1;return 1;
}
int MotorControl_SetTarget(MotorControl *m, float rpm)
{if (!(rpm>=0 && rpm<=m->config.max_rpm)) return 0;m->target_rpm=rpm;return 1;}
void MotorControl_Reverse(MotorControl *m) {m->requested_direction=(int8_t)-m->requested_direction;}
void MotorControl_ClearFault(MotorControl *m) {MotorControl_Stop(m);m->fault=MOTOR_FAULT_NONE;}
int32_t MotorControl_Delta16(uint16_t current, uint16_t previous)
{uint32_t d=(uint16_t)(current-previous);return d>=32768U ? (int32_t)d-65536 : (int32_t)d;}
int8_t MotorKnob_Sample(MotorKnob *k, uint8_t ab)
{
    static const int8_t table[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    uint8_t old=k->previous;uint8_t edges=k->edges?k->edges:4U;int8_t step;ab&=3;
    if ((old^ab)==3U) k->quarters=0;
    else k->quarters+=table[(old<<2)|ab];
    k->previous=ab;step=0;
    if(k->quarters>=(int8_t)edges){k->quarters=0;step=1;}
    if(k->quarters<=-(int8_t)edges){k->quarters=0;step=-1;}
    return step;
}
void MotorControl_Step(MotorControl *m, int32_t delta)
{
    float error, next_integral, unbounded, output, slope, magnitude;
    m->total_counts+=delta;
    if (!(m->config.counts_per_rev>0)) {m->raw_rpm=m->measured_rpm=0;ResetPid(m);return;}
    m->raw_rpm=(float)delta*60.0f/(m->config.counts_per_rev*DT);
    m->measured_rpm+=m->config.filter_alpha*(m->raw_rpm-m->measured_rpm);
    if (!m->enabled || m->fault) {ResetPid(m);return;}
    if(Abs(m->raw_rpm)>m->config.max_rpm*1.5f+30.0f)m->overspeed_ms+=10U;else m->overspeed_ms=0;
    if(m->overspeed_ms>=100U){Fail(m,MOTOR_FAULT_OVERSPEED);return;}
    if (m->requested_direction!=m->active_direction || m->reversing) {
        if (!m->reversing) {m->reversing=1;m->reverse_ms=0;m->quiet_ms=0;ResetPid(m);}
        m->reverse_ms+=10;
        m->quiet_ms=Abs(m->raw_rpm)<10.0f ? (uint16_t)(m->quiet_ms+10U) : 0;
        if (m->quiet_ms>=100U) {m->active_direction=m->requested_direction;m->reversing=0;ResetPid(m);}
        else if(m->reverse_ms>=3000U) Fail(m,MOTOR_FAULT_REVERSE_TIMEOUT);
        return;
    }
    if (m->target_rpm==0) {ResetPid(m);return;}
    slope=m->config.ramp_rpm_s*DT;
    if (m->ramped_rpm<m->target_rpm) m->ramped_rpm=Clamp(m->ramped_rpm+slope,0,m->target_rpm);
    else m->ramped_rpm=Clamp(m->ramped_rpm-slope,m->target_rpm,m->config.max_rpm);
    magnitude=m->measured_rpm*m->active_direction;
    error=m->ramped_rpm-magnitude;
    next_integral=Clamp(m->integral+m->config.ki*error*DT,-m->config.max_pwm,m->config.max_pwm);
    unbounded=m->config.kp*error+next_integral-m->config.kd*(magnitude-m->previous_rpm)/DT;
    output=Clamp(unbounded,0,m->config.max_pwm);
    /* Conditional integration: integrate only if unsaturated or unwinding. */
    if (unbounded==output || (unbounded>output && error<0) || (unbounded<output && error>0)) m->integral=next_integral;
    m->previous_rpm=magnitude;
    m->pwm=(int16_t)(output*m->active_direction);
    /* A lone/noisy edge must not keep a disconnected Hall loop alive at full PWM.
     * Require net forward progress, not absolute jitter, every300ms under effort.
     * Five percent of the current ramp reference is a permissive stall threshold,
     * not a tracking-quality criterion. Two counts minimum reject isolated toggles. */
    if (Abs((float)m->pwm)>=200 && m->ramped_rpm>=1) {
        float minimum=m->ramped_rpm*m->config.counts_per_rev*(0.3f/60.0f)*0.05f;
        if(minimum<2.0f)minimum=2.0f;
        m->no_feedback_ms+=10U;
        m->feedback_progress+=(int64_t)delta*m->active_direction;
        if(m->no_feedback_ms>=300U){
            if((float)m->feedback_progress<minimum){Fail(m,MOTOR_FAULT_NO_FEEDBACK);return;}
            m->no_feedback_ms=0;m->feedback_progress=0;
        }
    }else {m->no_feedback_ms=0;m->feedback_progress=0;}
    if (magnitude < -15 && Abs((float)m->pwm)>100) m->wrong_sign_ms+=10;
    else m->wrong_sign_ms=0;
    if (m->wrong_sign_ms>=200) Fail(m,MOTOR_FAULT_FEEDBACK_SIGN);
}
