"""Offline pin ledger verification: no GPIO writes, serial ports or flashing."""
from pathlib import Path
import csv,hashlib,json,re,subprocess,sys
H=Path(__file__).resolve().parents[1];D=H/'Docs/引脚与接线'
subprocess.run([sys.executable,str(H/'Tools/generate_pin_reference.py'),'--check'],check=True)
data=json.loads((D/'引脚资源.json').read_text(encoding='utf-8'));roles=data['roles'];headers=data['headers']
from configuration_facts import board_source_text,uart_bindings
B=BD=board_source_text()
checked=[]
def bind(source,port,pins,statuses=None):
 m=re.search(r'^#define\s+'+port+r'\s+GPIO([A-H])\b',source,re.M);assert m,port
 for macro in pins:
  expr=re.search(r'^#define\s+'+macro+r'\s+(.+)$',source,re.M);assert expr,macro
  values=re.findall(r'GPIO_PIN_(\d+)\b',expr[1]);assert values,macro
  for n in values:
   p='P'+m[1]+n;assert p in roles and roles[p]['status']!='空闲候选',(macro,p)
   if statuses:assert roles[p]['status'] in statuses,(macro,p)
   checked.append(dict(macro=macro,pin=p))
for i in range(1,7):bind(B,f'BOARD_KEY{i}_PORT',[f'BOARD_KEY{i}_PIN'],['已分配'])
for name in ['HEARTBEAT','STATUS','FAULT']:bind(B,f'BOARD_LED_{name}_PORT',[f'BOARD_LED_{name}_PIN'])
for port,pins in [
 ('BOARD_LCD_SPI_PORT',['BOARD_LCD_SPI_PINS']),('BOARD_LCD_BL_PORT',['BOARD_LCD_BL_PIN']),('BOARD_LCD_DC_PORT',['BOARD_LCD_DC_PIN']),
 *[(f'BOARD_CAM_{g}_PORT',[f'BOARD_CAM_{g}_PINS']) for g in 'EDGCA'],
 *[(f'BOARD_CAM_{g}_PORT',[f'BOARD_CAM_{g}_PIN']) for g in ['PWDN','SCL','SDA']],
 ('BOARD_SD_DATA_PORT',['BOARD_SD_DATA_PINS']),('BOARD_SD_CMD_PORT',['BOARD_SD_CMD_PIN']),
 *[(f'BOARD_QSPI_{g}_PORT',[f'BOARD_QSPI_{g}_PIN']) for g in ['CLK','CS','IO0','IO1','IO2','IO3']],
 ('BOARD_PWM_PORT',['BOARD_PWM_PIN','BOARD_PWM2_PIN']),('BOARD_MOTOR_HALL_A_PORT',['BOARD_MOTOR_HALL_A_PIN']),('BOARD_MOTOR_HALL_B_PORT',['BOARD_MOTOR_HALL_B_PIN']),
 ('BOARD_MOTOR_PWM_PORT',['BOARD_MOTOR_PWM_PIN','BOARD_MOTOR_PWM2_PIN']),
 ('BOARD_MOTOR_DIR_PORT',['BOARD_MOTOR_IN1','BOARD_MOTOR_IN2','BOARD_MOTOR_STBY']),
 ('BOARD_MOTOR_KNOB_PORT',['BOARD_MOTOR_KNOB_A','BOARD_MOTOR_KNOB_B']),('BOARD_MOTOR_KEY_PORT',['BOARD_MOTOR_KEY_PIN'])]:bind(BD,port,pins)
assert re.search(r'BOARD_KEY_ACTIVE_STATE\s+GPIO_PIN_RESET',B)
assert re.search(r'BOARD_KEY_PULL\s+GPIO_PULLUP',B)
assert re.search(r'MOTOR_DRIVER_PROFILE\s+1\b',(H/'project/App/config/motor_config.h').read_text())
for p,n in [('PA0',10),('PB0',23),('PB12',47),('PB13',46)]:assert headers['U9'][n-1]==p
assert headers['U10'][6]=='PC13' and headers['U10'][56:58]==['PG2','PG3']
assert headers['U10'][:4]==['GND']*4
assert headers['H1'][2:4]==['GND','+5V']
# Preserve all 108 previously reviewed connector-to-signal facts, while improving status.
old=json.loads((D/'证据/旧版108引出表.json').read_text(encoding='utf-8'))
for row in old:assert headers[row['Connector']][int(row['HeaderPin'])-1]==row['Pin']
resources=(H/'project/App/platform/board_resources.c').read_text(encoding='utf-8')
assert uart_bindings()=={'SERVO':'huart2','HOST':'huart1','BLUETOOTH':'huart3'}
for role in ['SERVO','HOST','BLUETOOTH']:
 bind(B,'BOARD_UART_'+role+'_TX_PORT',['BOARD_UART_'+role+'_TX_PIN'])
 bind(B,'BOARD_UART_'+role+'_RX_PORT',['BOARD_UART_'+role+'_RX_PIN'])
bind(B,'BOARD_USB_GPIO_PORT',['BOARD_USB_GPIO_PINS'])
# Check actual cross-device ownership, excluding inactive bridge-profile pins.
from configuration_facts import configuration_facts
_,facts,_,_=configuration_facts()
profile=int(re.search(r'MOTOR_DRIVER_PROFILE\s+(\d+)',(H/'project/App/config/motor_config.h').read_text())[1])
skip=set()
if profile!=2:skip.add('BOARD_MOTOR_STBY')
if profile!=3:skip.add('BOARD_MOTOR_PWM2_PIN')
if profile==3:skip.update(['BOARD_MOTOR_IN1','BOARD_MOTOR_IN2'])
if profile==0:skip.update(['BOARD_MOTOR_PWM_PIN','BOARD_MOTOR_PWM2_PIN','BOARD_MOTOR_IN1','BOARD_MOTOR_IN2','BOARD_MOTOR_STBY'])
owners={}
for binding in checked:
 if binding['macro'] in skip:continue
 pin=binding['pin']
 assert pin not in owners,('GPIO conflict',pin,owners.get(pin),binding['macro'])
 owners[pin]=binding['macro']
timers=[facts[x] for x in ['BOARD_MOTOR_HALL_TIM','BOARD_MOTOR_TICK_TIM','BOARD_PWM_INSTANCE']]
if profile:timers.append(facts['BOARD_MOTOR_PWM_TIM'])
assert len(timers)==len(set(timers)),('Timer conflict',timers)
legacy=list(csv.DictReader((H/'Docs/适配规划/03_硬件规划/设备引脚分配.csv').open(encoding='utf-8-sig')))
assert not any('12V' in x['电源/备注'].replace('旧通用12V备注','') for x in legacy if x['设备']=='舵机STS3215-C001')
assert any(x['MCU引脚']=='PG8' and 'SPI6_NSS' in x['外设/复用'] for x in legacy)
assert all(roles[p]['status']!='空闲候选' for p in ['PB14','PB15','PE10','PE12','PE13','PG4','PG5','PB8','PB9','PA5','PC4'])
R=next(p for p in H.parents if (p/'学习与项目导航.md').is_file())
pdf=R/'01_嵌入式技术资料库/01_芯片与内核资料/09_硬件信息/芯路城_鹿小班_STM32H743ZIT6/01_原理图/LXB743ZI-P1原理图.pdf'
assert pdf.is_file()
report=dict(date='2026-10-24',scope='schematic mapping + source assignments + generated docs; no hardware operations',macro_pin_checks=len(checked),active_gpio_ownerships_checked=len(owners),exclusive_timers_checked=len(timers),macro_bindings=checked,previous_header_gpio_facts_checked=108,header_positions=120,h1_positions=8,free_candidates=36,schematic_sha256=hashlib.sha256(pdf.read_bytes()).hexdigest(),source_files_checked=len(data['source_hash']),source_modified=False,physical_test=False,passed=True)
(D/'源码与排针核验.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='macro_bindings'},ensure_ascii=False))
