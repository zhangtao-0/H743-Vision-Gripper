"""Negative configuration tests for actual NV backend and capability declarations; no device IO."""
from pathlib import Path
import subprocess,tempfile,runpy,json
H=Path(__file__).resolve().parents[1];A=H/'project/App';E=H/'Docs/解耦与移植/20261024_配置分层重构';GCC=r'C:\Dev-Cpp\MinGW64\bin\gcc.exe'
TEST=r'''
#include <assert.h>
#include <stdio.h>
#include "platform_hal.h"
#include "board_nv_storage.h"
HostFlashRegs host_flash_regs;
static unsigned touched;
HAL_StatusTypeDef HAL_FLASH_Unlock(void){touched++;return HAL_ERROR;}
HAL_StatusTypeDef HAL_FLASH_Lock(void){touched++;return HAL_ERROR;}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef*p,uint32_t*e){(void)p;(void)e;touched++;return HAL_ERROR;}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;touched++;return HAL_ERROR;}
int main(void){
 unsigned char data[32]={0};
 assert(!BoardNv_Read((BoardNvRegion)99,data,32));
 assert(!BoardNv_Read(BOARD_NV_PROFILE_A,data,0));
 assert(!BoardNv_Rewrite(BOARD_NV_PROFILE_A,0,32));
 assert(!BoardNv_Rewrite(BOARD_NV_PROFILE_A,data,3));
 assert(!BoardNv_Rewrite(BOARD_NV_PROFILE_A,data,0xFFFFFFFFU));
#ifdef BAD_MAP
 assert(!BoardNv_Read(BOARD_NV_PROFILE_A,data,32));
 assert(!BoardNv_Rewrite(BOARD_NV_PROFILE_A,data,32));
#endif
 assert(!touched);puts("PASS region/length/partition guard rejects before any Flash operation");return 0;
}
'''
def main():
 cases=[];hal=runpy.run_path(str(H/'Tools/run_integration_host_tests.py'))['HAL']
 with tempfile.TemporaryDirectory() as folder:
  p=Path(folder);(p/'platform_hal.h').write_text(hal);(p/'test.c').write_text(TEST)
  variants={'valid':'','alias':'#undef BOARD_LIMIT_FLASH_A\n#define BOARD_LIMIT_FLASH_A BOARD_PROFILE_FLASH_A\n','code_area':'#undef BOARD_PROFILE_FLASH_A\n#define BOARD_PROFILE_FLASH_A 0x08000000UL\n','wrong_bank':'#undef BOARD_PROFILE_FLASH_BANK\n#define BOARD_PROFILE_FLASH_BANK 1U\n'}
  for name,override in variants.items():
   (p/'test_board.h').write_text('#include "boards/lxb743zi_p1/board_profile.h"\n'+override)
   (p/'test_target.h').write_text('#define APP_BOARD_PROFILE_HEADER "test_board.h"\n#define PLATFORM_NV_BACKEND "mcu/stm32h743/nv_backend.inc"\n')
   args=[GCC,'-std=c99','-Wall','-Wextra','-Werror','-Wno-unknown-pragmas','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-DAPP_TARGET_CONFIG_HEADER="test_target.h"']
   if name!='valid':args+=['-DBAD_MAP=1']
   for inc in [p,A/'config',A/'platform']:args+=['-I',str(inc)]
   args += [str(p/'test.c'),str(A/'platform/board_nv_storage.c'),'-o',str(p/'test.exe')]
   b=subprocess.run(args,capture_output=True,text=True)
   if b.returncode:raise RuntimeError(name+'\n'+b.stdout+b.stderr)
   r=subprocess.run([str(p/'test.exe')],capture_output=True,text=True);r.check_returncode();print(name,r.stdout,end='');cases.append({'case':name,'passed':True})
  # Configuration-only feature gate: this does not claim an SDK without DCMI can compile the whole H7 app.
  (p/'cap.c').write_text('#include "integration_config.h"\nint main(void){return 0;}\n')
  (p/'test_board.h').write_text('#include "boards/lxb743zi_p1/board_profile.h"\n#undef BOARD_HAS_DCMI\n#define BOARD_HAS_DCMI 0\n')
  base=[GCC,'-fsyntax-only','-DAPP_TARGET_CONFIG_HEADER="test_target.h"']
  for inc in [p,A/'config',A/'platform']:base+=['-I',str(inc)]
  bad=subprocess.run([*base,str(p/'cap.c')],capture_output=True,text=True)
  assert bad.returncode and 'no DCMI' in bad.stderr
  good=subprocess.run([*base,'-DBOARD_ENABLE_CAMERA=0',str(p/'cap.c')],capture_output=True,text=True)
  assert not good.returncode,good.stderr
  cases.append({'case':'missing DCMI requires explicit disable','passed':True});print('PASS missing DCMI is rejected; explicit camera disable passes configuration gate')
 (E/'配置与Flash边界测试.json').write_text(json.dumps({'scope':'actual C NV backend + fake HAL; negative maps, no Flash/board operations','passed':True,'cases':cases},ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':main()
