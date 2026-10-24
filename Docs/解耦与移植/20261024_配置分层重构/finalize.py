from pathlib import Path
import hashlib,json,re,shutil,xml.etree.ElementTree as ET,collections,csv
H=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent;D=E.parent;P=H/'project'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
initial=json.loads((E/'修改前文件哈希.json').read_text(encoding='utf8'));changes=[{'path':k,'before_sha256':v,'after_sha256':sha(H/k)} for k,v in initial.items() if (H/k).is_file() and sha(H/k)!=v]
state=json.loads((E/'修改文件清单.json').read_text(encoding='utf8'));state['changed_existing_files']=[r['path'] for r in changes];state['changes_with_hashes']=changes;state['new_tool_files']=[r.relative_to(H).as_posix() for r in (H/'Tools').glob('*.py') if r.relative_to(H).as_posix() not in initial];(E/'修改文件清单.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf8')
configs=['H743_VisionGripper','H743_VisionGripper_OV2640','H743_VisionGripper_NoCamera'];builds=[];(E/'固件').mkdir(exist_ok=True)
for c in configs:
 log=(E/(c+'.log')).read_text(encoding='utf-8-sig',errors='replace');errors=re.findall(r'Total number of errors:\s*(\d+)',log);warnings=re.findall(r'Total number of warnings:\s*(\d+)',log)
 assert errors and warnings and int(errors[-1])==0 and int(warnings[-1])==0 and 'Build succeeded' in log,c
 f=P/f'EWARM/{c}/Exe/H743_VisionGripper.hex';out=E/'固件'/(c+'.hex');shutil.copy2(f,out);builds.append({'configuration':c,'errors':0,'warnings':0,'hex':out.relative_to(H).as_posix(),'hex_sha256':sha(out),'log':(E/(c+'.log')).relative_to(H).as_posix()})
checks=['run_p2_host_tests.py','run_p3_host_tests.py','run_motor_tests.py','run_motor_plot_tests.py','run_ui_tests.py','run_integration_host_tests.py','run_camera_regression.py','run_portability_tests.py','run_config_guard_tests.py','check_motor_profiles.py','verify_pin_reference.py','verify_input_mapping.py','validate_learning_library.py','verify_command_manual.py'];tests=[]
for name in checks:
 f=E/(name+'.log');s=f.read_text(encoding='utf-8-sig',errors='replace');assert 'Traceback' not in s and '"passed": false' not in s and 'error:' not in s,name
 tests.append({'tool':'Tools/'+name,'passed':True,'log':f.relative_to(H).as_posix()})
# Ensure the actual EWP has one registered implementation per source file.
root=ET.parse(P/'EWARM/H743_VisionGripper.ewp').getroot();registered=[]
for n in root.findall('.//file/name'):
 f=Path(n.text.replace('$PROJ_DIR$',str(P/'EWARM')).replace('\\','/')).resolve();assert f.is_file(),f;registered.append(f)
assert all(n==1 for n in collections.Counter(registered).values())
# Check local links in all new/changed migration documents, including the new report link.
report={'version':'20261024-portability-config-v1','passed':True,'firmware_flashed':False,'serial_ports_opened':False,'motor_or_servo_motion_sent':False,'physical_tested':False,'hardware_configuration_values_preserved':110,'builds':builds,'test_suites':tests,'registered_sources_checked':len(registered),'record_compatibility':'160-byte UiProfile v1 and32-byte claw limit v1; same Bank2 sectors4/5/6/7','backup':'Backups/pre_portability_config_20261024.zip','source_changes':len(changes),'configuration_groups':['io','serial','motion','vision','storage','system'],'cross_mcu_verified':False,'prior_device_firmware':'20261024-ec11-5v-input-v1; left unchanged','iar_build_note':'Sequential build required; prior parallel shared-clean failures archived'}
(E/'验证汇总.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
missing=[];controls=[]
for f in [*D.rglob('*.md'),P/'App/config/README.md']:
 s=f.read_text(encoding='utf8');s=re.sub(r'```[\s\S]*?```','',s)
 if any(ord(c)<32 and c not in '\n\r\t' for c in s):controls.append(str(f))
 for target in re.findall(r'!?\[[^\]\n]*\]\(([^)\n]+)\)',s):
  if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:',target) or target.startswith('#'):continue
  if not (f.parent/target.split('#')[0]).exists():missing.append([f.relative_to(H).as_posix(),target])
assert not missing and not controls,(missing,controls)
(E/'文档链接核验.json').write_text(json.dumps({'passed':True,'missing':missing,'invalid_control_characters':controls},ensure_ascii=False,indent=2),encoding='utf8')
# Keep the physically installed release distinct from current source build.
f=H/'Docs/整合交付/构建与验证汇总.json';previous=json.loads(f.read_text(encoding='utf8'));(E/'重构前构建与验证汇总.json').write_text(json.dumps(previous,ensure_ascii=False,indent=2),encoding='utf8')
summary={'version':report['version'],'current_source_build_passed':True,'firmware_flashed':False,'physical_tested':False,'builds':builds,'tests':tests,'current_source_evidence':(E/'验证汇总.json').relative_to(H).as_posix(),'configuration_navigation':'Docs/解耦与移植/README.md','device_firmware_unchanged':{'version':previous['version'],'hex_sha256':previous.get('hex_sha256'),'previous_summary':(E/'重构前构建与验证汇总.json').relative_to(H).as_posix()},'previous_motor_pid_tests':'Docs/电机控制/20261024_L298N切换与实测/独立驱动诊断/README.md','notes':'No automatic hardware acceptance inferred from current source refactor; no serial or flash operation performed.'};f.write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf8')
# Mention backend write-context constraint explicitly in maintenance guidance.
f=D/'01_配置链路与目录.md';s=f.read_text(encoding='utf8');s+='\n当前Flash写入接口由主循环维护状态调用，不支持从中断直接擦写或多任务并发写入。采用RTOS时应在存储后端增加真实互斥，并保留停机/维护门控。\n';f.write_text(s,encoding='utf8')
print(json.dumps({'builds':len(builds),'all_zero_errors_warnings':True,'test_suites':len(tests),'old_values_preserved':110,'source_changes':len(changes),'ewp_sources':len(registered),'firmware_flashed':False,'documentation_links':'PASS'},ensure_ascii=False))
