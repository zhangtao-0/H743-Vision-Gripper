"""Check input maps, executable help and VOFA byte presets without using serial ports."""
from pathlib import Path
import csv,json,re,subprocess,sys,hashlib
H=Path(__file__).resolve().parents[1];D=H/'Docs/输入映射'
subprocess.run([sys.executable,str(H/'Tools/generate_console_help.py'),'--check'],check=True)
groups=json.loads((D/'指令帮助目录.json').read_text(encoding='utf-8'));assert len(groups)==9
presets=json.loads((D/'VOFA快捷指令.json').read_text(encoding='utf-8'));rows=list(csv.DictReader((D/'VOFA快捷指令.csv').open(encoding='utf-8-sig')));assert len(rows)==len(presets)
hex_count=text_count=0
for x in presets:
 raw=bytes.fromhex(x['bytes_hex'])
 if x['mode']=='ASCII+CRLF':assert raw==(x['command']+'\r\n').encode('ascii');text_count+=1
 else:
  hex_count+=1;assert raw==bytes.fromhex(x['command']) and len(raw)==raw[3]+4
  if raw[4]!=0xF5:assert sum(raw[2:])&255==255
  else:assert len(raw)==11 and raw[-1]==0
for x,y in zip(rows,presets):assert x['command']==y['command'] and x['bytes_hex']==y['bytes_hex']
keymap=json.loads((D/'按键映射.json').read_text(encoding='utf-8'));assert [x['id'] for x in keymap[:7]]==[1,2,3,4,5,6,7]
assert len(list(csv.DictReader((D/'按键映射.csv').open(encoding='utf-8-sig'))))==len(keymap)
params=list(csv.DictReader((D/'参数映射.csv').open(encoding='utf-8-sig')));assert [int(x['id']) for x in params]==list(range(25))
source=(H/'project/App/modules/ui/ui_port.c').read_text(encoding='utf-8');names=re.findall(r'"([^"]+)"',re.search(r'params\[\]=\{(.*?)\};',source).group(1))
assert [r['ui_name'] for r in params]==names
from configuration_facts import board_source_text
board=board_source_text()
for i,port,pin in [(1,'GPIOA',0),(2,'GPIOB',0),(3,'GPIOB',12),(4,'GPIOB',13),(5,'GPIOC',0),(6,'GPIOC',1)]:
 assert re.search(r'BOARD_KEY'+str(i)+r'_PORT\s+'+port,board)
 assert re.search(r'BOARD_KEY'+str(i)+r'_PIN\s+GPIO_PIN_'+str(pin)+r'\b',board)
report=dict(version='20261023-debug-grants-v1',help_categories=9,text_examples=text_count,hex_examples=hex_count,presets=len(presets),key_rows=len(keymap),parameter_rows=len(params),generated_header_consistent=True,serial_ports_opened=False,hardware_test=False,passed=True)
(D/'映射表核验.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,ensure_ascii=False))
