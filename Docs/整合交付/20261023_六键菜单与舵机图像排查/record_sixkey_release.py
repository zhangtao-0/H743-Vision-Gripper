# Archived one-off release script. Current guides include subsequent hardware corrections.
"""Record reviewed release deltas and refresh documentation references only."""
from pathlib import Path
import hashlib,json,re,zipfile,datetime
H=Path(__file__).resolve().parents[3];ROOT=H.parents[3]
E=H/'Docs/整合交付/20261023_六键菜单与舵机图像排查'
T=H/'Docs/教学手册/05_维护与证据';C=H/'Docs/指令手册/04_维护与证据'
def read(p):return p.read_text(encoding='utf-8-sig')
def data(p):return json.loads(read(p))
def save(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

# Only these reviewed production files may differ from the previous baseline.
approved={
'App/app_main.c','App/bsp/bsp_key.c','App/bsp/bsp_key.h','App/bsp/input_control.h',
'App/platform/boards/lxb743zi_p1/board_config.h','App/modules/integration/integration_console.c',
'App/modules/integration/console_help.h','App/modules/motor/motor_service.c',
'App/modules/servo_p3/servo_p3.c','App/modules/servo_p3/servo_p3.h',
'App/drivers/Servo/feetech_sts_bus.h','App/drivers/Servo/feetech_sts_bus.c',
'App/drivers/Servo/feetech_sts_command.c','App/modules/ui/ui_core.c',
'App/modules/ui/ui_input.h','App/modules/ui/ui_input.c','App/modules/ui/ui_service.c','App/modules/ui/ui_service.h'}
baseline=data(T/'固件源码基线.json');changes=[]
for rel,old in baseline.items():
 new=sha(H/'project'/rel)
 if old!=new:
  assert rel in approved, 'Unreviewed production drift: '+rel
  changes.append(dict(path=rel,before=old,after=new));baseline[rel]=new
if changes:
 save(E/'生产源码变更.json',changes)
 save(T/'固件源码基线.json',baseline)

# Find the same definition text from the saved pre-edit file, then use a
# definition-shaped line only if its signature actually changed.
locations=data(T/'源码函数定位.json')
with zipfile.ZipFile(H/'Backups/pre_sixkey_servo_video_20261023.zip') as backup:
 for row in locations:
  p=H/'project'/row['path'];lines=read(p).splitlines()
  if sha(p)!=row['source_sha256']:
   try:old=backup.read('project/'+row['path']).decode('utf-8-sig').splitlines()[row['line']-1]
   except KeyError:old=''
   matches=[i+1 for i,line in enumerate(lines) if old and line==old]
   if len(matches)!=1:
    matches=[i+1 for i,line in enumerate(lines) if re.search(r'\b'+re.escape(row['symbol'])+r'\s*\(',line) and not line.lstrip().startswith(('//','/*','*')) and not line.strip().endswith(';') and re.match(r'^(?:static\s+)?(?:const\s+)?[\w*]+\s+',line)]
   assert len(matches)==1,(row,matches)
   row['line']=matches[0];row['source_sha256']=sha(p)
save(T/'源码函数定位.json',locations)

source=read(H/'project/App/modules/integration/integration_console.c')
catalog=data(C/'文本指令目录.json');known={r['parser_pattern'] for r in catalog}
for pattern,command in [('servo status','@servo status'),('servo scan %23s %23s','@servo scan BAUD'),('servo angle %23s %23s','@servo angle DEG')]:
 if pattern not in known:catalog.append(dict(command=command,parser_pattern=pattern,source='App/modules/integration/integration_console.c',line=0,inputs=['USART1 HOST 115200 8N1','USB CDC'],bluetooth_supported=False))
for row in catalog:
 token='"'+row['parser_pattern']+'"'
 row['line']=next(i+1 for i,line in enumerate(source.splitlines()) if token in line)
save(C/'文本指令目录.json',sorted(catalog,key=lambda r:r['command']))
snapshot=data(C/'指令源码快照.json')
for rel in snapshot['local']:snapshot['local'][rel]=sha(H/'project'/rel)
for rel in sorted(approved):snapshot['local'][rel]=sha(H/'project'/rel)
snapshot['date']='2026-10-23';snapshot['local_release']='20261023-sixkey-servo-v2'
snapshot['observed_release']='20261023-sixkey-servo-v2 source; hardware observation separately recorded'
snapshot['previous_snapshot']='Backups/pre_sixkey_servo_video_20261023.zip';save(C/'指令源码快照.json',snapshot)

pins=H/'Docs/引脚与接线/引脚资源.json';d=data(pins)
for rel in d['source_hash']:d['source_hash'][rel]=sha(H/'project'/rel)
for pin in ('PA2','PA3'):
 d['roles'][pin]['wiring']='本实物转接板同名丝印接：PA2 TX→板TX、PA3 RX→板RX；共地。其他转接板须核对丝印命名方向'
save(pins,d)

registry=data(T/'手册清单.json')
for folder in [H/'Docs/视觉识别与抓取',H/'Docs/指令手册/01_文本控制台',E]:
 for p in folder.glob('*.md'):
  rel=p.relative_to(ROOT).as_posix()
  if not any(r['path']==rel for r in registry.values()):registry['p:'+p.relative_to(H).as_posix()]=dict(path=rel,kind='p',title=read(p).splitlines()[0].removeprefix('# '))
save(T/'手册清单.json',registry)
print('Reviewed production delta, function locations, command catalog and pin references recorded.')
