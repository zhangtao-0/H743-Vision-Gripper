from pathlib import Path
import json,hashlib,re,shutil
H=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent
hx=E/'H743_EC11_5V_NOPULL.hex';dump=(E/'独立读回.bin').read_bytes();upper=0;n=0;bad=[]
for line in hx.read_text().splitlines():
 b=bytes.fromhex(line[1:]);assert sum(b)%256==0;typ=b[3];addr=int.from_bytes(b[1:3],'big');data=b[4:4+b[0]]
 if typ==4:upper=int.from_bytes(data,'big')<<16
 elif typ==0:
  for i,v in enumerate(data):
   off=upper+addr+i-0x08000000
   if not 0<=off<len(dump) or dump[off]!=v:bad.append(hex(upper+addr+i))
   n+=1
assert not bad
status=json.loads((E/'最终停机核验.json').read_text(encoding='utf8'))
assert all(x in status['@motor status'] for x in ['on=0','pwm=0','fault=0'])
report={'version':'20261024-ec11-5v-input-v1','firmware_flashed':True,'programmer_verified':True,'independent_readback_verified':True,'hex_sha256':hashlib.sha256(hx.read_bytes()).hexdigest(),'payload_bytes_compared':n,'mismatches':bad,'build_errors':0,'build_warnings':0,'driver_profiles_compile_passed':[0,1,2,3],'host_motor_tests_passed':True,'pin_validation_passed':True,'teaching_validation_passed':True,'pin_map':{'A':'PG2/U10-57','C':'PC13/U10-7','B':'PG3/U10-58'},'user_confirmed_supply':'5V','user_confirmed_C':'press output; polarity not measured','input_pull':'GPIO_NOPULL','physical_EC11_tested':False,'motor_started_this_revision':False,'final_motor_status':status['@motor status'],'final_debug_status':status['@debug status'],'gpio_register_probe':'HOT-mode probe returnedFFFFFFFF; not accepted as runtime GPIO configuration verification. Rely on source/build/flashed HEX verification; raw failed probe retained.'}
(E/'验证汇总.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
p=E/'README.md';s=p.read_text(encoding='utf8');title,rest=s.split('\n',1);p.write_text(title+'\n\n> 修正版已烧录并校验，独立读回全部HEX数据一致；三路输入NOPULL已通过源码、构建与固件核验。当前停机、fault0、Motor/STS授权均OFF。本轮未启动电机，实体EC11尚待接线测试。\n'+rest,encoding='utf8')
p=H/'Docs/整合交付/构建与验证汇总.json';previous=json.loads(p.read_text(encoding='utf8'));shutil.copy2(p,E/'前版构建与验证汇总.json');previous.update(version=report['version'],hex='Docs/电机控制/20261024_EC11五伏模块接线/H743_EC11_5V_NOPULL.hex',hex_sha256=report['hex_sha256'],programmer_verified=True,independent_readback_verified=True,physical_ec11_verified=False,physical_pid_tracking_passed=False,prior_revision_pid_tracking_passed=True,prior_revision_pid_evidence='Docs/电机控制/20261024_L298N切换与实测/独立驱动诊断/README.md',current_revision_note='Only EC11 GPIO pull configuration changed; motor algorithm unchanged. No motor motion test on this HEX.',current_revision_evidence='Docs/电机控制/20261024_EC11五伏模块接线/验证汇总.json');p.write_text(json.dumps(previous,ensure_ascii=False,indent=2),encoding='utf8')
p=H/'Docs/电机控制/01_硬件参数与按设备接线.md';s=p.read_text(encoding='utf8').replace('本方案用 3.3V','当前接5V，数字输入为FT且无内部上下拉').replace('| A | PE9 | U9-36，TIM1_CH1 AF1 |','| A | PA8 | U10-47，TIM1_CH1 AF1 |').replace('| 编码器 VCC | 板上确认的 3.3V 电源 | 不接 7.4V | 根据截图避免把内部上拉升至 5V |','| 编码器 VCC | 板上+5V | 不接动力电源 | PA8/PE11为FT输入，内部上下拉禁用 |').replace('## 设备4：用户已提供TB6612 H桥','## 设备4：H桥接口与历史TB6612方案').replace('profile=2，当前TB6612配置','profile=2，历史TB6612配置');title,rest=s.split('\n',1);p.write_text(title+'\n\n> 当前驱动为L298N、PB14→ENA（非PE13），霍尔A=PA8、B=PE11；EC11五伏接法见[最新旋钮名单](20261024_EC11五伏模块接线/README.md)。文末TB6612电气部分属于旧驱动方案。\n'+rest,encoding='utf8')
# Check all newly written local links.
for name in ['README.md']:
 p=E/name
 for t in re.findall(r'\]\(([^)]+)\)',p.read_text(encoding='utf8')):
  if re.match(r'^https?://',t):continue
  assert (p.parent/t).exists(),t
print(json.dumps({'readback_payload_bytes':n,'mismatches':0,'new_manual_links':'PASS','flashed':True,'physical_knob_tested':False},ensure_ascii=False))
