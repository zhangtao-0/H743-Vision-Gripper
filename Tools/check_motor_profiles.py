"""Compile all real H7 H-bridge adapter branches with the configured IAR toolchain."""
from pathlib import Path
import subprocess,json,tempfile,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
project=ET.parse(ROOT/'project/EWARM/H743_VisionGripper.ewp').getroot()
includes=[]
for option in project.find('configuration').findall('.//option'):
 if option.findtext('name')=='CCIncludePath2':
  for state in option.findall('state'):
   includes+=['-I',state.text.replace('$PROJ_DIR$',str(ROOT/'project/EWARM')).replace('\\','/')]
compiler=r'C:\IAR Systems\Embedded Workbench 9.2\arm\bin\iccarm.exe'
reports=[]
with tempfile.TemporaryDirectory() as folder:
 for profile in [0,1,2,3]:
  command=[compiler,str(ROOT/'project/App/platform/board_motor.c'),'--cpu=Cortex-M7','--fpu=VFPv5_d16','--endian=little','-e','-DSTM32H743xx','-DUSE_HAL_DRIVER',f'-DMOTOR_DRIVER_PROFILE={profile}','--dlib_config',r'C:\IAR Systems\Embedded Workbench 9.2\arm\inc\c\DLib_Config_Full.h',*includes,'-o',str(Path(folder)/f'profile_{profile}.o')]
  run=subprocess.run(command,capture_output=True,text=True)
  output=run.stdout+run.stderr
  (ROOT/f'Docs/电机控制/profile_{profile}.log').write_text(output,encoding='utf-8')
  reports.append({'profile':profile,'exit_code':run.returncode,'warnings_none':'Warnings: none' in output})
  print('Motor H-bridge profile',profile,'exit',run.returncode)
(ROOT/'Docs/电机控制/驱动适配编译.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(x['exit_code']==0 and x['warnings_none'] for x in reports)
