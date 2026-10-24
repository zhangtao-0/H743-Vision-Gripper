from pathlib import Path
import hashlib,json,re,sys,zipfile,collections,csv
H=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent;P=H/'project';sys.path.insert(0,str(H/'Tools'))
from configuration_facts import configuration_facts
raw,resolved,sources,headers=configuration_facts()
assert raw['BOARD_UART_SERVO_BAUD']=='1000000U'
# Compare all old board wiring definitions with their new effective values.
old={}
with zipfile.ZipFile(H/'Backups/pre_portability_config_20261024.zip') as z:
 for rel in ['project/App/platform/boards/lxb743zi_p1/board_config.h','project/App/platform/boards/lxb743zi_p1/board_devices.h']:
  text=z.read(rel).decode('utf-8-sig')
  for m in re.finditer(r'^#define[ \t]+(BOARD_\w+)[ \t]+([^\n]+)',text,re.M):old[m[1]]=m[2].split('/*')[0].strip()
def norm(x):return re.sub(r'(?<=\d)[UL]+\b','',re.sub(r'\s+','',x))
diff={k:[v,resolved.get(k)] for k,v in old.items() if k not in resolved or norm(v)!=norm(resolved[k])}
assert not diff,diff
(E/'原板硬件配置等值核验.json').write_text(json.dumps({'old_macros_checked':len(old),'new_effective_macros':len(resolved),'changed_existing_macro_values':diff,'passed':True,'scope':'source configuration comparison; excludes physical runtime verification'},ensure_ascii=False,indent=2),encoding='utf8')
initial=json.loads((E/'修改前文件哈希.json').read_text(encoding='utf8'))
changes=[]
for rel,before in initial.items():
 p=H/rel
 if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()!=before:changes.append(rel)
# Update exactly the source dependencies that were intentionally changed.
p=H/'Docs/引脚与接线/引脚资源.json';d=json.loads(p.read_text(encoding='utf8'));(E/'引脚资源_修改前.json').write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf8')
for rel,sha in list(d['source_hash'].items()):
 f=P/rel;now=hashlib.sha256(f.read_bytes()).hexdigest()
 if now!=sha:
  assert initial.get('project/'+rel)==sha,('unreviewed pin baseline drift',rel)
  d['source_hash'][rel]=now
for f in headers|{P/'App/config/target_config.h',P/'App/config/targets/stm32h743_lxb.h',P/'App/platform/board_config.h',P/'App/platform/board_devices.h'}:
 d['source_hash'][f.relative_to(P).as_posix()]=hashlib.sha256(f.read_bytes()).hexdigest()
# Source columns now identify the actual grouped facts rather than compatibility wrappers.
for pin,r in d['roles'].items():
 if pin in ['PA0','PB0','PB12','PB13','PC0','PC1','PG7','PG0','PG1']:group='io'
 elif pin in ['PA2','PA3','PA9','PA10','PB10','PB11','PA11','PA12']:group='serial'
 elif pin in ['PA8','PE11','PB14','PB15','PE10','PE12','PE13','PG2','PG3','PC13','PD12','PD13']:group='motion'
 else:continue
 r['source']='App/platform/boards/lxb743zi_p1/board_'+group+'_cfg.h'
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf8')
# Reviewed production baseline: keep SDK files unchanged and register the added port/config sources.
base_dir=H/'Docs/教学手册/05_维护与证据';p=base_dir/'固件源码基线.json';base=json.loads(p.read_text(encoding='utf-8-sig'));(E/'教学基线_修改前.json').write_text(json.dumps(base,ensure_ascii=False,indent=2),encoding='utf8')
for rel,before in list(base.items()):
 f=P/rel;now=hashlib.sha256(f.read_bytes()).hexdigest()
 if now!=before:
  assert initial.get('project/'+rel)==before,('unreviewed teaching drift',rel)
  base[rel]=now
for f in (P/'App').rglob('*'):
 if f.suffix in ['.c','.h','.inc'] and f.is_file():base[f.relative_to(P).as_posix()]=hashlib.sha256(f.read_bytes()).hexdigest()
p.write_text(json.dumps(base,ensure_ascii=False,indent=2),encoding='utf8')
p=base_dir/'源码函数定位.json';rows=json.loads(p.read_text(encoding='utf-8-sig'));(E/'源码函数定位_修改前.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
rename={('Core/Src/main.c','MX_USART2_UART_Init'):('App/platform/mcu/stm32h743/uart_backend.inc','BoardUart_ConfigureHardware'),('Core/Src/stm32h7xx_it.c','USART2_IRQHandler'):('App/platform/mcu/stm32h743/uart_backend.inc','BoardUart_Irq'),('Core/Src/stm32h7xx_it.c','TIM6_DAC_IRQHandler'):('Core/Src/stm32h7xx_it.c','BOARD_MOTOR_TICK_HANDLER'),('Core/Src/stm32h7xx_it.c','DCMI_IRQHandler'):('Core/Src/stm32h7xx_it.c','BOARD_CAMERA_HANDLER'),('Core/Src/stm32h7xx_it.c','DMA2_Stream7_IRQHandler'):('Core/Src/stm32h7xx_it.c','BOARD_CAMERA_DMA_HANDLER')}
for r in rows:
 if (r['path'],r['symbol']) in rename:r['path'],r['symbol']=rename[(r['path'],r['symbol'])]
for rel,symbol in [('App/platform/board_gpio.c','BoardGpio_EnableClock'),('App/platform/mcu/stm32h743/nv_backend.inc','BoardNv_Read'),('App/platform/mcu/stm32h743/nv_backend.inc','BoardNv_Rewrite'),('App/platform/mcu/stm32h743/uart_backend.inc','BoardUart_HardwareMspInit'),('App/platform/board_peripherals.c','BoardStartupGpio_Init')]:
 if not any(r['path']==rel and r['symbol']==symbol for r in rows):rows.append({'path':rel,'symbol':symbol})
for r in rows:
 f=P/r['path'];text=f.read_text(encoding='utf-8-sig');pattern=r'^[\w \t*]+\b'+re.escape(r['symbol'])+r'\s*\([^;{}]*\)\s*\{';m=re.search(pattern,text,re.M)
 if not m:raise RuntimeError('No current function definition: '+str(r))
 r['line']=text[:m.start()].count('\n')+1;r['source_sha256']=hashlib.sha256(f.read_bytes()).hexdigest()
p.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
# Refresh the existing source index without dropping non-function configuration entries.
p=H/'Docs/教学手册/03_源码阅读/02_源码定位索引.md';text=p.read_text(encoding='utf8');paths=re.findall(r'\| \[([^\]]+)\]\(\.\./\.\./\.\./project/',text)
paths+=list(dict.fromkeys(r['path'] for r in rows if r['path'] not in paths));paths+=sorted(f.relative_to(P).as_posix() for f in headers if f.relative_to(P).as_posix() not in paths)
body='# 当前工程源码定位索引\n\n核对版本：2026-10-24 portability-config-v1。UART初始化移入芯片适配层，IRQ由角色表展开；旧板型board_config/board_devices是兼容入口。点击文件后按函数名搜索。\n\n| 源文件 | 重点函数与定义行 |\n|---|---|\n'
for rel in paths:
 if not (P/rel).is_file():continue
 items=[f"`{r['symbol']}`：{r['line']}行" for r in rows if r['path']==rel]
 body+=f'| [{rel}](../../../project/{rel}) | '+('；'.join(items) if items else '配置/启动/库对接，按分层说明阅读')+' |\n'
p.write_text(body,encoding='utf8')
(E/'修改文件清单.json').write_text(json.dumps({'changed_existing_files':changes,'original_hardware_macro_values_unchanged':len(old),'new_source_files':[p for p in base if 'project/'+p not in initial and p.startswith('App/')],'record_format':'UiProfile160bytes version1; claw limit32bytes version1; addresses unchanged','firmware_flashed':False},ensure_ascii=False,indent=2),encoding='utf8')
# Machine-readable configuration navigation.
with (H/'Docs/解耦与移植/配置宏索引.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['配置宏','当前原始值','展开后的值','唯一配置文件'])
 for k in sorted(raw):w.writerow([k,raw[k],resolved[k],sources[k]])
print('Preserved',len(old),'old hardware values;',len(changes),'existing files changed;',len(rows),'function definitions refreshed')
