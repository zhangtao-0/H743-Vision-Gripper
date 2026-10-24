"""Read reviewed default board profile headers for documentation, not as a C compiler replacement."""
from pathlib import Path
import re
H=Path(__file__).resolve().parents[1]
def configuration_facts():
 target=H/'project/App/config/targets/stm32h743_lxb.h'
 entry=re.search(r'^#define APP_BOARD_PROFILE_HEADER "([^"]+)"',target.read_text(encoding='utf8'),re.M)[1]
 root=H/'project/App/platform';macros={};sources={};seen=set()
 def visit(p):
  p=p.resolve()
  if p in seen:return
  seen.add(p);s=p.read_text(encoding='utf-8-sig');s=s.replace('\\\n',' ')
  for inc in re.findall(r'^#include "([^"]+)"',s,re.M):
   q=p.parent/inc
   if q.is_file():visit(q)
  for m in re.finditer(r'^#define[ \t]+([A-Za-z_][A-Za-z_0-9]*)(\([^\n]*?\))?[ \t]+([^\n]+)',s,re.M):
   name=m[1]
   if m[2]:continue
   macros[name]=m[3].split('/*')[0].strip();sources[name]=p.relative_to(H).as_posix()
 visit(root/entry)
 def expand(name,trail=()):
  if name not in macros:return name
  if name in trail:raise ValueError('Recursive configuration macro: '+name)
  return re.sub(r'\b[A-Za-z_]\w*\b',lambda m:expand(m[0],trail+(name,)) if m[0] in macros else m[0],macros[name])
 resolved={name:expand(name) for name in macros}
 return macros,resolved,sources,seen

def board_source_text():
 _,resolved,_,_=configuration_facts()
 return '\n'.join('#define '+k+' '+v for k,v in resolved.items())

def uart_bindings():
 s=(H/'project/App/platform/boards/lxb743zi_p1/board_serial_cfg.h').read_text(encoding='utf8').replace('\\\n',' ')
 m=re.search(r'^#define BOARD_UART_BINDINGS\(X\)\s+(.+)',s,re.M)
 return dict(re.findall(r'X\((\w+),\s*(\w+)\)',m[1]))
