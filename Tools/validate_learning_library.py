"""Validate teaching documents only; never modifies production firmware."""
from pathlib import Path
import hashlib,json,re,sys,datetime
H=Path(__file__).resolve().parents[1]
ROOT=H.parents[3]
P=H/'Docs/教学手册'
E=P/'05_维护与证据'
def read(p):return p.read_text(encoding='utf-8-sig',errors='replace')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def data(name):return json.loads(read(E/name))
issues=[];links=0;source_links=0;characters=0
registry=data('手册清单.json')
files=[ROOT/r['path'] for r in registry.values()]+[ROOT/'学习与项目导航.md']
for f in files:
 if not f.is_file():issues.append('Missing document: '+str(f));continue
 s=read(f);characters+=len(s)
 if '{{' in s:issues.append('Unresolved token: '+str(f))
 if not s.startswith('# '):issues.append('Missing title: '+str(f))
 # Ignore fenced code blocks; links there are illustrative text.
 s=re.sub(r'```[\s\S]*?```','',s)
 for target in re.findall(r'!?\[[^\]\n]*\]\(([^)\n]+)\)',s):
  from urllib.parse import unquote
  target=target.strip().strip('<>')
  if re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:',target) or target.startswith('#'):continue
  target=unquote(target.split('#',1)[0]);dest=(f.parent/target).resolve();links+=1
  if H/'project' in dest.parents:source_links+=1
  if not dest.exists() and dest!=(E/'资料核验报告.json').resolve():issues.append('Broken link: '+str(f.relative_to(ROOT))+' -> '+target)
baseline=data('固件源码基线.json');changed=[]
for rel,sha in baseline.items():
 f=H/'project'/rel
 if not f.is_file() or digest(f)!=sha:changed.append(rel)
issues.extend('Production baseline changed: '+x for x in changed)
hals=data('HAL接口引用.json')
for row in hals:
 for key in ('header','source'):
  f=H/'project'/row[key]
  if not re.search(r'\b'+re.escape(row['symbol'])+r'\b',read(f)):issues.append('HAL symbol absent: '+str(row))
functions=data('源码函数定位.json')
for row in functions:
 f=H/'project'/row['path'];lines=read(f).splitlines();line=row['line']
 if digest(f)!=row['source_sha256'] or not (0<line<=len(lines)) or not re.search(r'\b'+re.escape(row['symbol'])+r'\s*\(',lines[line-1]):issues.append('Stale function location: '+str(row))
builds=data('示例编译结果.json')
for row in builds:
 log=read(E/row['log'])
 if row['exit_code'] or 'Errors: none' not in log or 'Warnings: none' not in log:issues.append('Example compile not clean: '+row['example'])
for ewp in (H/'project/EWARM').glob('*.ewp'):
 if re.search(r'lesson_(gpio|uart2)\.c',read(ewp)):issues.append('Learning example registered in production: '+str(ewp))
report=dict(observed_at=datetime.datetime.now().isoformat(timespec='seconds'),scope='New registered learning documents and source snapshot; excludes unrelated historical navigation links',registered_documents=len(registry),checked_markdown_files=len(files),characters=characters,internal_links=links,source_links=source_links,production_files_checked=len(baseline),production_changes=changed,hal_references=len(hals),function_locations=len(functions),example_compile_only=len(builds),firmware_flashed=False,issues=issues,passed=not issues)
(E/'资料核验报告.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
sys.exit(bool(issues))
