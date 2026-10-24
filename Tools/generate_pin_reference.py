"""Generate board pin tables from reviewed facts; --check does not write files."""
from pathlib import Path
import collections,csv,hashlib,io,json,re,sys
H=Path(__file__).resolve().parents[1];D=H/'Docs/引脚与接线'
data=json.loads((D/'引脚资源.json').read_text(encoding='utf-8'))
roles=data['roles'];headers=data['headers'];check='--check' in sys.argv
for path,sha in data['source_hash'].items():
 assert hashlib.sha256((H/'project'/path).read_bytes()).hexdigest()==sha,'Source drift; review before regenerating: '+path
assert len(headers['U9'])==len(headers['U10'])==60 and len(headers['H1'])==8
assert headers['U10'][:4]==['GND']*4
assert headers['U9'][:4]==['3.3V','3.3V','+5V','+5V']
assert headers['H1'][2:4]==['GND','+5V']
locations=collections.defaultdict(list)
for c,rows in headers.items():
 for n,pin in enumerate(rows,1):locations[pin].append(c+'-'+str(n))
exported=[p for c in ['U9','U10'] for p in headers[c] if re.fullmatch(r'P[A-H]\d+(?:_C)?',p)]
assert len(exported)==len(set(exported))==108
power_notes={'3.3V':'3.3V逻辑电源，非GPIO', '+5V':'5V电源，非GPIO，不接按键到地', 'GND':'公共地；常开按键另一端可接这里', 'VREF':'模拟参考，非普通供电/GPIO', 'VBAT':'RTC备份电源，非普通电源输出/GPIO', 'NRST':'复位网，H1-7经R2 1kΩ，不作业务键'}
def item(pin):
 if pin in roles:return roles[pin]
 if pin in power_notes:return dict(device='电源/专用',signal=pin,status='电源/地/专用',mode='非GPIO',wiring=power_notes[pin],other_connector='',source='原理图p1')
 assert re.fullmatch(r'P[A-H]\d+',pin),pin
 note='源码未分配、原理图未见板载业务负载；先确认实物无外接，新增后登记和初始化'
 if pin in ['PD14','PD15']:note+='；若用TIM4_CH3/4，必须服从现有TIM4的50Hz时基'
 if pin in ['PE7','PE8','PE14','PE15']:note+='；TIM1已用于电机编码器，不代表该定时器空闲'
 return dict(device='待分配',signal='普通GPIO候选',status='空闲候选',mode='未配置业务功能',wiring=note,other_connector='',source='原理图+当前源码核对')
outputs={}
def put(path,text):outputs[path]=text
def csv_text(rows,fields):
 s=io.StringIO(newline='');w=csv.DictWriter(s,fieldnames=fields);w.writeheader();w.writerows(rows);return '\ufeff'+s.getvalue()
fields=['连接器','脚号','信号/MCU引脚','状态','设备','用途','模式/复用','其他插座','接线与限制','实物状态']
rows=[]
for c in ['U9','U10','H1']:
 for n,p in enumerate(headers[c],1):
  x=item(p);rows.append(dict(zip(fields,[c,n,p,x['status'],x['device'],x['signal'],x['mode'],x.get('other_connector',''),x['wiring'],'原理图/源码确认；本轮未逐脚通断测试'])))
put(D/'按板子排针顺序.csv',csv_text(rows,fields))
body='''# 按开发板排针顺序的引脚总表

依据LXB743ZI-P1原理图p1与2026-10-23默认工程。U9/U10各2×30，完整120个脚位；另列H1八针。**脚号是连接器编号，不是芯片封装脚号。**

按原理图1/2、3/4逐行排列，圆圈标Pin1。实物需按丝印/Pin1及通断定位，不能把左右列直接套在桌上照片。见[排针原理图](证据/U9_U10原理图排针.png)、[U10顶部放大](证据/U10顶部供电核对.png)。

**U9-1/2=3.3V，U9-3/4=5V，U9-13/14=GND；U10-1/2/3/4全部GND。H1-3=GND，H1-4=5V。**

已分配/板载专用不重复占用；条件预留即使未初始化也不直接释放；接口候选预留尚未初始化但保留方案；空闲候选不等于已确认你的实物没有额外接线。

'''
for c in ['U9','U10','H1']:
 body+='## '+c+'\n\n| 奇数脚 | 信号 | 状态 | 设备/用途 | 偶数脚 | 信号 | 状态 | 设备/用途 |\n|---|---|---|---|---|---|---|---|\n'
 for n in range(1,len(headers[c])+1,2):
  a,b=headers[c][n-1:n+1];x,y=item(a),item(b)
  body+=f"| {n} | {a} | {x['status']} | {x['device']} / {x['signal']} | {n+1} | {b} | {y['status']} | {y['device']} / {y['signal']} |\n"
 body+='\n'
put(D/'02_按板子排针顺序.md',body+'完整限制见[CSV](按板子排针顺序.csv)。返回[总入口](README.md)。\n')
free=[p for p in exported if item(p)['status']=='空闲候选']
counts=collections.Counter(item(p)['status'] for p in exported)
body=f'''# 当前空闲候选与扩展预留

U9/U10有108个MCU命名引出信号，其中PC2_C/PC3_C是特殊模拟焊盘。当前筛出 **{len(free)}个普通GPIO空闲候选**，不是已完成电气/复用/实物验证的保证。关闭软件功能不自动释放板上焊接负载。

| 状态 | 引出信号数 |
|---|---|
'''
for status,n in sorted(counts.items()):body+=f'| {status} | {n} |\n'
body+='\n## 普通GPIO空闲候选\n\n新增普通输入/输出优先从本表选。若要使用UART/SPI/定时器，还必须核对AF、外设实例、通道、时基、IRQ和DMA。\n\n| GPIO | 排针 | 注意事项 |\n|---|---|---|\n'
for p in sorted(free,key=lambda p:(p[1],int(p[2:]))):body+=f"| {p} | {', '.join(locations[p])} | {item(p)['wiring']} |\n"
body+='''
## 不列为空闲的预留

| 预留组 | 引脚 | 状态 |
|---|---|---|
| 后续限位/反馈 | PG4、PG5 | 明确保留 |
| 多控制器候选 | PB8 RX、PB9 TX | UART4 AF8候选，尚未初始化/接线；改用需先调整规划 |
| H桥 | PB14、PB15、PE10、PE12、PE13 | profile1：PB14 ENA PWM、PE10/12方向；PE13仅TB6612 profile2预留，PB15仅profile3备用 |
| 摄像头 | PC4、PA5 | 已接FPC1，模块定义待确认 |
| 调试扩展 | PA15、PB3、PB4 | JTAG/SWO复用核对前保守预留 |
| 主调试/晶振 | PA13/PA14、PH0/PH1、PC14/PC15 | 不作普通业务扩展 |
| 特殊模拟焊盘 | PC2_C、PC3_C | 不直接当PC2/PC3普通GPIO使用 |

## 还要检查外设资源

| 资源 | 当前责任 |
|---|---|
| USART1/2/3 | VOFA/STS/JDY-31 |
| SPI6 | LCD，包括PG8硬件NSS |
| DCMI+DMA2_Stream7 | 摄像头采集 |
| TIM1 | MG310正交编码器，不能另设PWM时基 |
| TIM4 CH1/2 | PD12/PD13 PWM舵机50Hz，其它通道不能单独另设频率 |
| TIM6+TIM6_DAC_IRQn | 电机1ms调度；新增DAC需统一分发共享IRQ |
| TIM12 | L298N A通道ENA 20kHz，profile1 |
| USB OTG FS | PA11/PA12，原生USB CDC |
| QSPI/SDMMC1/RTC | 板载Flash/TF/时钟 |

本表不将未列的其它外设自动宣布为空闲。维护见[更新流程](05_来源维护与修订记录.md)，返回[总入口](README.md)。
'''
put(D/'04_空闲与预留资源.md',body)
allpins=[f'P{port}{n}' for port in 'ABCDEFG' for n in range(16)]+['PH0','PH1','PC2_C','PC3_C']
def key(p):return (p[1],int(re.search(r'\d+',p).group()),p.endswith('_C'))
portrows=[]
body='''# 按GPIO端口名称查引脚

PA→PB→…的索引视图。含114个普通GPIO名称及PC2_C/PC3_C两个特殊模拟焊盘记录，**不是116个独立可用GPIO**。没有排针位置的脚不作为普通接线候选；_C焊盘须查模拟开关和芯片资料。

| GPIO/焊盘 | 排针/H1 | 状态 | 用途 |
|---|---|---|---|
'''
for p in sorted(allpins,key=key):
 x=item(p);loc=', '.join(locations[p]) or '未从U9/U10/H1普通引出'
 portrows.append(dict(GPIO=p,连接器=loc,状态=x['status'],设备=x['device'],用途=x['signal'],说明=x['wiring']))
 body+=f"| {p} | {loc} | {x['status']} | {x['device']} / {x['signal']} |\n"
put(D/'03_按GPIO名称索引.md',body+'\n返回[总入口](README.md)，排针物理顺序看[板子表](02_按板子排针顺序.md)。\n')
put(D/'按GPIO名称索引.csv',csv_text(portrows,list(portrows[0])))
body='''# 按外设查接线与引脚

本表说明当前工程的设备职责与连接，不代表每件外设已接好/实测。电源见[硬件档案](../硬件档案/README.md)；按键先看[直接接线步骤](01_按键接线与测试.md)。方向以MCU为参照，TX接对端RX。

'''
device_rows=[]
for dev in dict.fromkeys(x['device'] for x in roles.values()):
 body+='## '+dev+'\n\n| 信号 | MCU引脚 | 排针/H1 | 专用插座 | 模式/状态 | 接线与限制 |\n|---|---|---|---|---|---|\n'
 for p,x in roles.items():
  if x['device']!=dev:continue
  loc=', '.join(locations[p]) or '未普通引出'
  body+=f"| {x['signal']} | {p} | {loc} | {x.get('other_connector','') or '—'} | {x['mode']}；{x['status']} | {x['wiring']} |\n"
  device_rows.append(dict(设备=dev,信号=x['signal'],MCU引脚=p,排针=loc,专用插座=x.get('other_connector',''),状态=x['status'],模式=x['mode'],接线=x['wiring'],来源=x.get('source','')))
 body+='\n'
body+='''## 电源、地与非GPIO信号

| 设施 | 接线 |
|---|---|
| 常开按键 | 一端GPIO，一端GND；默认内部上拉，不接5V |
| U9 | 1/2=3.3V，3/4=5V，13/14=GND，12=VREF |
| U10 | 1/2/3/4=GND，13=VBAT，非普通电源输出 |
| U7屏幕 | 9/10=3.3V，1/3/8=GND，11/12机械地；配套FPC |
| FPC1相机 | 1/2/3=3.3V，4/5/8/24=GND，25/26机械地，23=NC |
| MG310 | 编码器VCC=3.3V；M+/M−接H桥OUT1/OUT2，不能接GPIO；VM独立供电 |
| STS3215-C001 | UART接TTL转接板；舵机按C001已确认规格供电，见硬件档案，不用历史通用12V备注 |
| PWM舵机 | 信号PD12/PD13；动力独立供电并共地，信号线不是电源 |
| H1 | 1=SWCLK、2=SWDIO、3=GND、4=5V、5=PA10 RX、6=PA9 TX、7=NRST经1kΩ、8=3.3V |
| 板载按键 | RST、BOOT0是复位/启动控制，不是KEY1–5 |

可筛选[按外设接线CSV](按外设接线.csv)。所有视图由同一JSON生成，维护见[更新流程](05_来源维护与修订记录.md)。
'''
put(D/'00_按外设接线.md',body)
put(D/'按外设接线.csv',csv_text(device_rows,list(device_rows[0])))
old=[]
for c in ['U9','U10']:
 for n,p in enumerate(headers[c],1):
  if p not in exported:continue
  x=item(p);old.append(dict(Pin=p,Connector=c,HeaderPin=n,Role=x['device']+' / '+x['signal'],Status=x['status'],PhysicalTest='本轮未实测',Evidence='Docs/引脚与接线/引脚资源.json; schematic p1 + source'))
put(H/'Docs/适配规划/03_硬件规划/板级引出与用途表.csv',csv_text(old,list(old[0])))
for path,text in outputs.items():
 if check:assert path.is_file() and path.read_bytes()==text.encode('utf-8'),'Generated output differs: '+str(path)
 else:path.write_bytes(text.encode('utf-8'))
report=dict(date=data['date'],board=data['board'],header_positions=120,h1_positions=8,exported_mcu_named_signals=108,ordinary_exported_gpio=106,special_analog_pads=2,port_index_records=116,free_candidates=len(free),status_counts=dict(counts),device_signal_rows=len(device_rows),generated_files=len(outputs),source_files_checked=len(data['source_hash']),production_changes=False,physical_test=False,passed=True)
if not check:(D/'表格核验.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
