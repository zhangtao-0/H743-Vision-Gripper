"""Host-only tests for actual UI model/input/profile; never accesses hardware."""
from pathlib import Path
import subprocess,tempfile,json
H=Path(__file__).resolve().parents[1]
GCC=r'C:\Dev-Cpp\MinGW64\bin\gcc.exe'
def main():
 with tempfile.TemporaryDirectory() as folder:
  executable=Path(folder)/'ui_tests.exe';ui=H/'project/App/modules/ui'
  args=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-I',str(ui),str(H/'Tests/ui/test_ui.c'),*[str(ui/n) for n in ['ui_core.c','ui_input.c','ui_profile.c']],'-o',str(executable)]
  build=subprocess.run(args,capture_output=True,text=True)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(executable)],capture_output=True,text=True)
  evidence=H/'Docs/屏幕交互';evidence.mkdir(exist_ok=True)
  (evidence/'主机状态机测试.json').write_text(json.dumps(dict(scope='actual C navigation/input/profile, no MCU or Flash test',exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr),ensure_ascii=False,indent=2),encoding='utf-8')
  scenes=[]
  for line in run.stdout.splitlines():
   if line.startswith('SCENE|'):scenes.append(dict(name=line.split('|')[1],rows=[]))
   elif line.startswith('ROW|'):
    _,index,chinese,selected,text=line.split('|',4);scenes[-1]['rows'].append(dict(index=int(index),chinese=int(chinese),selected=int(selected),text=text))
  (evidence/'页面模型快照.json').write_text(json.dumps(scenes,ensure_ascii=False,indent=2),encoding='utf-8')
  print('\n'.join(x for x in run.stdout.splitlines() if x.startswith('PASS')));print(run.stderr,end='');run.check_returncode()
  args=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-I',str(H/'Tests/ui/stubs'),'-I',str(ui),str(H/'Tests/ui/test_settings.c'),str(ui/'ui_settings.c'),str(ui/'ui_profile.c'),'-o',str(executable)]
  build=subprocess.run(args,capture_output=True,text=True)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(executable)],capture_output=True,text=True)
  (evidence/'主机开机配置测试.json').write_text(json.dumps(dict(scope='actual C settings service with simulated storage/actuator APIs; no physical Flash test',exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr),ensure_ascii=False,indent=2),encoding='utf-8')
  print(run.stdout,run.stderr,end='');run.check_returncode()


  import runpy
  hal=runpy.run_path(str(H/'Tools/run_integration_host_tests.py'))['HAL']
  extra="""
#define FLASH_SECTOR_4 4U
#define FLASH_SECTOR_5 5U
typedef struct {uint32_t CCR;} HostScb;
extern HostScb host_scb;
#define SCB (&host_scb)
#define SCB_CCR_DC_Msk 0x10000U
void SCB_InvalidateDCache_by_Addr(uint32_t*,int32_t);
"""
  (Path(folder)/'platform_hal.h').write_text(hal.replace('#endif',extra+'\n#endif'),encoding='utf-8')
  args=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-Wno-unknown-pragmas','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-Wl,--image-base,0x400000','-I',folder,'-I',str(ui),'-I',str(H/'project/App/config'),'-I',str(H/'project/App/platform'),str(H/'Tests/ui/test_profile_store.c'),str(ui/'ui_profile.c'),str(ui/'ui_profile_store.c'),str(H/'project/App/platform/board_nv_storage.c'),'-o',str(executable)]
  build=subprocess.run(args,capture_output=True,text=True)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(executable)],capture_output=True,text=True)
  (evidence/'主机配置Flash模型测试.json').write_text(json.dumps(dict(scope='actual H7 profile store with Windows virtual memory/HAL fault model; no hardware erase/program',exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr),ensure_ascii=False,indent=2),encoding='utf-8')
  print(run.stdout,run.stderr,end='');run.check_returncode()

if __name__=='__main__':main()
