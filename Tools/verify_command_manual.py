"""Offline documentation audit. Never opens serial ports or sends commands."""
from pathlib import Path
import hashlib,json,re,struct,math,subprocess,sys,datetime
H=Path(__file__).resolve().parents[1]
D=H/'Docs/指令手册';E=D/'04_维护与证据'
def read(f):return f.read_text(encoding='utf-8-sig')
def data(name):return json.loads(read(E/name))
issues=[]
snapshot=data('指令源码快照.json')
for rel,sha in snapshot['local'].items():
    f=H/'project'/rel
    if not f.exists() or hashlib.sha256(f.read_bytes()).hexdigest()!=sha:issues.append('Source drift: '+rel)
s=read(H/'project/App/modules/integration/integration_console.c')
patterns={x for x in re.findall(r'(?:strcmp|strncmp|sscanf)\(s,\s*"([^"]+)"',s) if not x.endswith(' ')}
catalog=data('文本指令目录.json')
if patterns!={row['parser_pattern'] for row in catalog}:issues.append('Command catalog does not match Execute branches')
text='\n'.join(read(f) for f in D.rglob('*.md'))
for row in catalog:
    if row['command'] not in text:issues.append('Command undocumented: '+row['command'])
examples=data('二进制示例.json')
for row in examples:
    b=bytes.fromhex(row['hex'])
    if len(b)!=b[3]+4:issues.append('Length incorrect: '+row['hex']);continue
    if b[4]==0xF5:
        if len(b)!=11 or b[2]!=254 or b[3]!=7 or b[5]>5 or b[-1]!=0 or not math.isfinite(struct.unpack('<f',b[6:10])[0]):issues.append('Bad F5: '+row['hex'])
    elif row.get('expected_valid',True):
        if (sum(b[2:])&255)!=255:issues.append('Checksum incorrect: '+row['hex'])
    elif (sum(b[2:])&255)==255:issues.append('Historical negative example unexpectedly valid: '+row['hex'])
# Ensure literal frames added later cannot evade the registered examples.
inline=set()
for f in D.rglob('*.md'):
    for token in re.findall(r'`([^`\n]+)`',read(f)):
        for value in token.split(' / '):
            if re.fullmatch(r'FF FF(?: [0-9A-F]{2}){4,}',value):inline.add(value)
if inline!={x['hex'] for x in examples}:issues.append('Inline examples differ from catalog')
cli_cases=[(['ping','0'],'FF FF 00 02 01 FC'),(['read','0','0x38','2'],'FF FF 00 04 02 38 02 BF'),(['f5','0','90'],'FF FF FE 07 F5 00 00 00 B4 42 00'),(['local','F2','02','04','84','03'],'FF FF FE 06 F2 02 04 84 03 7C')]
for args,expected in cli_cases:
    result=subprocess.run([sys.executable,str(H/'Tools/command_frame.py'),*args],capture_output=True,text=True)
    if result.returncode or result.stdout.strip()!=expected:issues.append('Offline CLI mismatch: '+str(args))
host=read(H/'project/App/modules/claw_core/claw_host_protocol.h')
if len(re.findall(r'^#define CLAW_HOST_CMD_(?:ANGLE|A0|B0|RANGE|PWM1_KEY|PWM2_KEY)\s',host,re.M))!=6:issues.append('F5 enum drift')
control=read(H/'project/App/drivers/Servo/feetech_sts_control.h')
block=control.split('FEETECH_STS_PARAM_OPEN_ANGLE = 0,',1)[1].split('FEETECH_STS_PARAM_COUNT',1)[0]
param_count=1+len(re.findall(r'^\s*FEETECH_STS_PARAM_[A-Z_]+\s*,',block,re.M))
if param_count!=21:issues.append('Parameter enum drift')
report=dict(checked_at=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(timespec='seconds'),scope='documentation, parser branch coverage, arithmetic and offline formatter; no hardware execution',upstream_commit=snapshot['upstream_head'],text_command_variants=len(catalog),binary_examples=len(examples),valid_binary_examples=sum(x.get('expected_valid',True) for x in examples),historical_invalid_examples=sum(not x.get('expected_valid',True) for x in examples),f5_subcommands=6,parameter_count=param_count,offline_formatter_cases=len(cli_cases),source_files_checked=len(snapshot['local']),source_changed_since_observed_release=any(x.startswith('Source drift:') for x in issues),serial_commands_sent=False,issues=issues,passed=not issues)
(E/'指令核验报告.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
sys.exit(bool(issues))
