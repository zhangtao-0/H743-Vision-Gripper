from pathlib import Path
import subprocess,hashlib,json,shutil
H=Path(__file__).resolve().parents[4]
E=Path(__file__).resolve().parent
src=H/'project/App/modules/motor/motor_control.c'
original=src.read_bytes()
(E/'motor_control_测试前原件.c').write_bytes(original)
s=original.decode('utf-8')
start=s.index('    slope=m->config.ramp_rpm_s*DT;')
# Diagnostic uses its own MCU timer deadline; normal PI and feedback window are not used.
s=s[:start]+'''    /* ONE-SHOT BENCH DIAGNOSTIC ONLY. Fixed70% duty, hard1s MCU deadline.
     * Never install as the production control algorithm. Boot remains disabled.
     * Existing service timing and control overspeed guards remain active. */
    m->no_feedback_ms+=10U;
    m->feedback_progress+=(int64_t)delta*m->active_direction;
    if(m->no_feedback_ms>=1000U) {
        Fail(m,MOTOR_FAULT_NONE);
        return;
    }
    m->ramped_rpm=m->target_rpm;
    m->pwm=(int16_t)(700*m->active_direction);
}
'''
# Keep only variables needed in this diagnostic build, avoid unused-variable warnings.
s=s.replace('float error, next_integral, unbounded, output, slope, magnitude;','/* fixed-output diagnostic; no PID temporaries */')
(E/'motor_control_临时诊断.c').write_text(s,encoding='utf-8')
try:
 src.write_text(s,encoding='utf-8')
 with (E/'诊断固件构建.log').open('w',encoding='utf-8') as log:
  r=subprocess.run([r'C:\IAR Systems\Embedded Workbench 9.2\common\bin\iarbuild.exe',str(H/'project/EWARM/H743_VisionGripper.ewp'),'-build','H743_VisionGripper','-log','all'],stdout=log,stderr=subprocess.STDOUT)
 if r.returncode:raise RuntimeError('IAR build failed: '+str(r.returncode))
 shutil.copy2(H/'project/EWARM/H743_VisionGripper/Exe/H743_VisionGripper.hex',E/'仅诊断_70PWM_1秒.hex')
 print('Diagnostic build complete')
finally:
 src.write_bytes(original)
 print('Production source restored:',hashlib.sha256(src.read_bytes()).hexdigest())
