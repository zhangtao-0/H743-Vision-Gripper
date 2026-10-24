from pathlib import Path
H=Path(__file__).resolve().parents[3];A=H/'project/App'
# Cache maintenance helper lets a HAL without a D-cache expose the same NV contract.
p=A/'platform/mcu/stm32h743/nv_backend.inc';s=p.read_text(encoding='utf8');pos=s.index('int BoardNv_Read(');s=s[:pos]+'''static void NvInvalidate(uint32_t address,uint32_t bytes){
#if defined(SCB_CCR_DC_Msk)
 if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)address,(int32_t)((bytes+31U)&~31U));
#else
 (void)address;(void)bytes;
#endif
}
'''+s[pos:];s=s.replace('if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)nv_slot[region].address,(int32_t)((bytes+31U)&~31U));','NvInvalidate(nv_slot[region].address,bytes);').replace('if(SCB->CCR&SCB_CCR_DC_Msk)SCB_InvalidateDCache_by_Addr((uint32_t *)nv_slot[region].address,(int32_t)bytes);','NvInvalidate(nv_slot[region].address,bytes);');p.write_text(s,encoding='utf8')
for name in ['run_p2_host_tests.py','run_p3_host_tests.py','run_motor_plot_tests.py','run_integration_host_tests.py','run_camera_regression.py']:
 p=H/'Tools'/name;s=p.read_text(encoding='utf8')
 # Existing byte-only regressions intentionally do not simulate MCU startup/MSP.
 if name in ['run_p2_host_tests.py','run_p3_host_tests.py']:
  s=s.replace('"-DSTM32H743xx"','"-DSTM32H743xx", "-DPLATFORM_TEST_UART_BYTE_ONLY=1"')
  s=s.replace('app / "platform",','app / "config", app / "platform",')
 if name=='run_p2_host_tests.py':s=s.replace('"platform/board_resources.c",','"platform/board_resources.c", "platform/board_gpio.c",')
 if name=='run_motor_plot_tests.py':s=s.replace("'platform','protocol'","'config','platform','protocol'")
 if name=='run_integration_host_tests.py':
  s=s.replace('#define FLASH_SECTOR_6 6U','#define FLASH_SECTOR_4 4U\n#define FLASH_SECTOR_5 5U\n#define FLASH_SECTOR_6 6U')
  s=s.replace('app / "config", app / "bsp"','app / "config", app / "platform", app / "bsp"')
  s=s.replace('str(app / "platform/claw_limit_storage_h7.c"),','str(app / "platform/claw_limit_storage_h7.c"), str(app / "platform/board_nv_storage.c"),')
  s=s.replace('app / "drivers/Servo", app / "bsp"','app / "drivers/Servo", app / "platform", app / "bsp"')
 if name=='run_camera_regression.py':
  # integration_config now selects a real board profile (macros only).
  s=s.replace("root/'project/App/config'","root/'project/App/config',root/'project/App/platform'") if False else s
 p.write_text(s,encoding='utf8')
p=H/'Tools/run_ui_tests.py';s=p.read_text(encoding='utf8');s=s.replace("'-I',str(H/'project/App/config'),str(H/'Tests/ui/test_profile_store.c')","'-I',str(H/'project/App/config'),'-I',str(H/'project/App/platform'),str(H/'Tests/ui/test_profile_store.c')");s=s.replace("str(ui/'ui_profile_store.c'),'-o'","str(ui/'ui_profile_store.c'),str(H/'project/App/platform/board_nv_storage.c'),'-o'");p.write_text(s,encoding='utf8')
print('Existing regressions now compile actual common ports; old test scope retained')
