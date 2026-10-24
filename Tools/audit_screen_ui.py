"""Audit the current screen-UI build and reviewed source closure; no flash writes."""
from pathlib import Path
import hashlib,json,re,xml.etree.ElementTree as ET
H=Path(__file__).resolve().parents[1];D=H/'Docs/屏幕交互'
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest()
root=ET.parse(H/'project/EWARM/H743_VisionGripper.ewp').getroot()
ui_files=[x.text for x in root.findall('.//file/name') if '/modules/ui/' in x.text]
assert len(ui_files)==7 and len(set(ui_files))==7
assert '#define MOTOR_DRIVER_PROFILE 0' in (H/'project/App/config/motor_config.h').read_text(encoding='utf-8')
assert '0x0817FFFF' in (H/'project/EWARM/stm32h743xx_flash.icf').read_text(encoding='utf-8')
changes=json.loads((D/'源文件变更.json').read_text(encoding='utf-8'))
for row in changes:assert sha(H/row['path'])==row['after'],row['path']
builds=[]
for config,logname in [('H743_VisionGripper','构建_default.log'),('H743_VisionGripper_OV2640','构建_OV2640.log'),('H743_VisionGripper_NoCamera','构建_NoCamera.log')]:
 log=(D/logname).read_text(encoding='utf-8-sig');errors=re.findall(r'Total number of errors:\s*(\d+)',log);warnings=re.findall(r'Total number of warnings:\s*(\d+)',log)
 assert errors[-1]=='0' and warnings[-1]=='0' and 'Build succeeded' in log
 assert (D/logname).stat().st_mtime>max((H/r['path']).stat().st_mtime for r in changes)
 firmware=H/f'project/EWARM/{config}/Exe/H743_VisionGripper.hex';upper=0;low=0xFFFFFFFF;high=0
 for line in firmware.read_text().splitlines():
  if not line.startswith(':'):continue
  record=bytes.fromhex(line[1:]);assert (sum(record)&255)==0
  size=record[0];address=int.from_bytes(record[1:3],'big');typ=record[3];data=record[4:4+size]
  if typ==4:upper=int.from_bytes(data,'big')<<16
  elif typ==2:upper=int.from_bytes(data,'big')<<4
  elif typ==0 and size:
   start=upper+address;end=start+size-1;assert 0x08000000<=start<=end<=0x0817FFFF;low=min(low,start);high=max(high,end)
 builds.append(dict(configuration=config,errors=0,warnings=0,hex=firmware.relative_to(H).as_posix(),hex_sha256=sha(firmware),data_address_range=[hex(low),hex(high)],log='Docs/屏幕交互/'+logname))
reports=['主机状态机测试.json','主机开机配置测试.json','主机配置Flash模型测试.json']
for name in reports:assert json.loads((D/name).read_text(encoding='utf-8'))['exit_code']==0
field_path=D/'现场验证.json';field=json.loads(field_path.read_text(encoding='utf-8')) if field_path.exists() else {}
matching=bool(field.get('flashed') and field.get('verified') and field.get('hex_sha256')==builds[0]['hex_sha256'])
result=dict(version='20261022-input-map-v1',reference_branch='codex/test-6101c97',reference_commit='0cf70fb0120a53d5c7caf75cd276cb3a366e8522',builds=builds,registered_ui_sources=len(ui_files),host_reports=reports,reviewed_production_changes=len(changes),motor_driver_profile=0,boot_profile_slots=['0x08180000','0x081A0000'],limit_slots=['0x081C0000','0x081E0000'],boot_load_erases_flash=False,boot_auto_enable=False,firmware_flashed=matching,physical_key_lcd_test=bool(matching and field.get('key_lcd_passed')),physical_flash_power_loss_test=bool(matching and field.get('flash_power_loss_passed')))
(D/'本版核验结果.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(builds=[(r['configuration'],r['errors'],r['warnings']) for r in builds],ui_sources=len(ui_files),reviewed_changes=len(changes),reserved_regions_excluded=True,firmware_flashed=matching),ensure_ascii=False))
