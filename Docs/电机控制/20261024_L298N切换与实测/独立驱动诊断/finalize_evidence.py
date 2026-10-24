from pathlib import Path
import hashlib,json,re,shutil
H=Path(__file__).resolve().parents[4];E=Path(__file__).resolve().parent
# Add a regression that uses the actual production ramp/config rather than the old simulated150RPM/s fixture.
p=H/'Tools/run_motor_tests.py';s=p.read_text(encoding='utf8');shutil.copy2(p,E/'run_motor_tests_修正前.py')
s=s.replace('#include "motor_control.h"','#include "motor_control.h"\n#include "motor_config.h"',1)
s=s.replace(' return 0;\n}\n\'\'\'',''' MotorConfig production={MOTOR_KP,MOTOR_KI,MOTOR_KD,MOTOR_ENCODER_COUNTS_PER_REV,
  MOTOR_MAX_RPM,MOTOR_RAMP_RPM_S,MOTOR_PWM_LIMIT,MOTOR_SPEED_FILTER};
 MotorControl_Init(&m,&production);MotorControl_SetTarget(&m,400);MotorControl_Enable(&m,1);
 speed=0;fraction=0;Plant(&m,&speed,&fraction,0,1200);
 assert(!m.fault&&fabsf(speed-400)<3);MotorControl_Stop(&m);
 MotorService_Stop();assert(MotorService_ZeroCount());
 uint16_t before=counter;counter=(uint16_t)(before-20);ticks(10);MotorService_Snapshot(&st);
 assert(st.control.total_counts==20&&st.control.raw_rpm>0); /* measured wiring: raw decrement = positive rotation */
 counter=before;ticks(10);MotorService_Snapshot(&st);assert(st.control.total_counts==0&&st.control.raw_rpm<0);
 puts("PASS production profile: configured ramp tracks400; measured negative raw encoder sign maps correctly in service");
 return 0;
}
\'\'\' ''')
p.write_text(s,encoding='utf8')
# Independently compare all Intel HEX payload bytes with the downloaded flash readback.
hx=E/'H743_L298N_反馈符号修正_快斜坡.hex';dump=(E/'修正版独立读回.bin').read_bytes();upper=0;n=0;bad=[]
for line in hx.read_text().splitlines():
 b=bytes.fromhex(line[1:]);assert sum(b)%256==0;kind=b[3];addr=int.from_bytes(b[1:3],'big');data=b[4:4+b[0]]
 if kind==4:upper=int.from_bytes(data,'big')<<16
 elif kind==0:
  for i,v in enumerate(data):
   off=upper+addr+i-0x08000000
   if not 0<=off<len(dump) or dump[off]!=v:bad.append(hex(upper+addr+i))
   n+=1
v={'hex_sha256':hashlib.sha256(hx.read_bytes()).hexdigest(),'payload_bytes_compared':n,'readback_bytes':len(dump),'mismatches':bad[:20],'passed':not bad}
(E/'修正版独立核验.json').write_text(json.dumps(v,indent=2),encoding='utf8');assert not bad;print('Readback:',v)
# Update just the reviewed config entry in the teaching baseline; leave all other hashes intact.
bp=H/'Docs/教学手册/05_维护与证据/固件源码基线.json';base=json.loads(bp.read_text(encoding='utf-8-sig'));shutil.copy2(bp,E/'教学基线_修正前.json');rel='App/config/motor_config.h';assert rel in base
base[rel]=hashlib.sha256((H/'project'/rel).read_bytes()).hexdigest();bp.write_text(json.dumps(base,ensure_ascii=False,indent=2),encoding='utf8')
# Current quick references; historical run reports are preserved.
p=H/'Docs/电机控制/02_软件配置与操作.md';s=p.read_text(encoding='utf8').replace('| MOTOR_ENCODER_SIGN | +1，反馈方向；机械正转计数为负时改 -1 |','| MOTOR_ENCODER_SIGN | -1；2026-10-24实测正向驱动时原始TIM1计数递减 |').replace('| MOTOR_RAMP_RPM_S | 150 RPM/s |','| MOTOR_RAMP_RPM_S | 1000 RPM/s；0→400参考斜坡约0.4秒 |');p.write_text(s,encoding='utf8')
p=H/'Docs/电机控制/04_VOFA旋钮按键与PID曲线.md';s=p.read_text(encoding='utf8');s=s.replace('参考斜坡150RPM/s仍生效，所以不是瞬间从0跳到400。4.5V是当前动力电源，低于电机额定7.4V；','当前参考斜坡1000RPM/s，0到400约0.4秒。2026-10-24实际使用L298N、用户报告VM=10.3V；原TB6612/4.5V为历史接线；');s=s.replace('先确认TB6612接线、电源限流、机械可自由转动。','先确认当前L298N接线、电源限流、机械可自由转动。');p.write_text(s,encoding='utf8')
print('Reviewed config baseline and current documentation updated')
