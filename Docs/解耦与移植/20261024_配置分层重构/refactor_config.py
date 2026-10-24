from pathlib import Path
import re
H=Path(__file__).resolve().parents[3];A=H/'project/App';B=A/'platform/boards/lxb743zi_p1';C=A/'config'
def put(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf8')
def header(name,body):
 guard=name.upper().replace('.','_');return '#ifndef '+guard+'\n#define '+guard+'\n'+body+'\n#endif\n'
old=(B/'board_config.h').read_text(encoding='utf8');devices=(B/'board_devices.h').read_text(encoding='utf8');integration=(C/'integration_config.h').read_text(encoding='utf8')
def defs(s,predicate):return '\n'.join(l for l in s.splitlines() if l.startswith('#define ') and predicate(l.split()[1]))+'\n'
io=defs(old,lambda n:n.startswith('BOARD_LED_') or n.startswith('BOARD_KEY'))
# Per-key properties preserve old global defaults and allow each key to differ.
for i in range(1,7):io+=f'#define BOARD_KEY{i}_PULL BOARD_KEY_PULL\n#define BOARD_KEY{i}_ACTIVE_STATE BOARD_KEY_ACTIVE_STATE\n'
vision=defs(devices,lambda n:n.startswith(('BOARD_LCD_','BOARD_CAM_')))
motion=defs(devices,lambda n:n.startswith(('BOARD_PWM','BOARD_MOTOR')))
motion+='''#define BOARD_MOTOR_KEY_ACTIVE_STATE GPIO_PIN_RESET
#define BOARD_MOTOR_TICK_HANDLER TIM6_DAC_IRQHandler
#define BOARD_MOTOR_TICK_PRIORITY 5U
#define BOARD_MOTOR_PWM_COUNTER_HZ 20000000U
#define BOARD_MOTOR_PWM_PERIOD 1000U
#define BOARD_MOTOR_TICK_COUNTER_HZ 1000000U
#define BOARD_MOTOR_TICK_HZ 1000U
#define BOARD_PWM_INSTANCE TIM4
#define BOARD_PWM_CLOCK_ENABLE() __HAL_RCC_TIM4_CLK_ENABLE()
#define BOARD_PWM_COUNTER_HZ 1000000U
#define BOARD_PWM_PERIOD_US 20000U
'''
storage=defs(devices,lambda n:n.startswith(('BOARD_SD_','BOARD_QSPI_')))+defs(integration,lambda n:n.startswith(('BOARD_QSPI_TEST','BOARD_PROFILE_FLASH','BOARD_LIMIT_FLASH')))
storage+='''#define BOARD_PROFILE_FLASH_BANK FLASH_BANK_2
#define BOARD_PROFILE_FLASH_SECTOR_A FLASH_SECTOR_4
#define BOARD_PROFILE_FLASH_SECTOR_B FLASH_SECTOR_5
#define BOARD_LIMIT_FLASH_BANK FLASH_BANK_2
#define BOARD_LIMIT_FLASH_SECTOR_A FLASH_SECTOR_6
#define BOARD_LIMIT_FLASH_SECTOR_B FLASH_SECTOR_7
#define BOARD_FLASH_PROGRAM_UNIT 32U
#define BOARD_FLASH_SECTOR_BYTES 0x20000UL
'''
system='''/* H743 platform facts, not business parameters. Startup/linker must agree. */
#define BOARD_PROFILE_NAME "LXB743ZI-P1"
#define BOARD_EXPECTED_PCLK1_HZ 100000000U
#define BOARD_EXPECTED_PCLK2_HZ 100000000U
#define BOARD_APB1_TIMER_KERNEL_HZ 200000000U
#define BOARD_APB2_TIMER_KERNEL_HZ 200000000U
#define BOARD_SYS_PLL_M 4U
#define BOARD_SYS_PLL_N 128U
#define BOARD_SYS_PLL_P 2U
#define BOARD_SYS_PLL_Q 2U
#define BOARD_SYS_PLL_R 2U
#define BOARD_HAS_DCMI 1
#define BOARD_HAS_USB_CDC 1
#define BOARD_HAS_SDMMC 1
#define BOARD_HAS_QSPI 1
#define BOARD_HAS_INTERNAL_NV 1
'''
serial=defs(old,lambda n:n.startswith(('BOARD_UART_','BOARD_SERVO_')))
serial+='''/* Single source of role->handle, initialization, GPIO mux and IRQ binding.
 * JDY-31/HC-08 both use BLUETOOTH role; UART wiring alone does not make their radio protocols equivalent. */
#define BOARD_UART_BINDINGS(X) \\
 X(SERVO, huart2) \\
 X(HOST, huart1) \\
 X(BLUETOOTH, huart3)
#define BOARD_UART_WORD_LENGTH UART_WORDLENGTH_8B
#define BOARD_UART_STOP_BITS UART_STOPBITS_1
#define BOARD_UART_PARITY UART_PARITY_NONE
#define BOARD_UART_MODE UART_MODE_TX_RX
#define BOARD_UART_FLOW_CONTROL UART_HWCONTROL_NONE
'''
for role,inst,port,tx,rx,speed,priority,group in [('HOST','USART1','A',9,10,'LOW',0,1),('SERVO','USART2','A',2,3,'HIGH',1,2),('BLUETOOTH','USART3','B',10,11,'LOW',2,2)]:
 serial+=f'''\n#define BOARD_UART_{role}_INSTANCE {inst}
#define BOARD_UART_{role}_TX_PORT GPIO{port}
#define BOARD_UART_{role}_TX_PIN GPIO_PIN_{tx}
#define BOARD_UART_{role}_RX_PORT GPIO{port}
#define BOARD_UART_{role}_RX_PIN GPIO_PIN_{rx}
#define BOARD_UART_{role}_AF GPIO_AF7_{inst}
#define BOARD_UART_{role}_PULL GPIO_NOPULL
#define BOARD_UART_{role}_SPEED GPIO_SPEED_FREQ_{speed}
#define BOARD_UART_{role}_IRQn {inst}_IRQn
#define BOARD_UART_{role}_HANDLER {inst}_IRQHandler
#define BOARD_UART_{role}_PRIORITY {priority}U
#define BOARD_UART_{role}_RCC_SELECTION RCC_PERIPHCLK_{inst}
#define BOARD_UART_{role}_RCC_GROUP {group}U
#define BOARD_UART_{role}_CLOCK_ENABLE() __HAL_RCC_{inst}_CLK_ENABLE()
#define BOARD_UART_{role}_CLOCK_DISABLE() __HAL_RCC_{inst}_CLK_DISABLE()
'''
for name,body in [('board_io_cfg.h',io),('board_serial_cfg.h',serial),('board_motion_cfg.h',motion),('board_vision_cfg.h',vision),('board_storage_cfg.h',storage),('board_system_cfg.h',system)]:put(B/name,header(name,body))
put(B/'board_profile.h',header('board_profile.h','''/* Only aggregate hardware configuration; no handles or initialization here. */
#include "board_system_cfg.h"
#include "board_io_cfg.h"
#include "board_serial_cfg.h"
#include "board_motion_cfg.h"
#include "board_vision_cfg.h"
#include "board_storage_cfg.h"
'''))
put(C/'target_config.h',header('target_config.h','''/* Override this one macro with a target header when adding another board/MCU. */
#ifndef APP_TARGET_CONFIG_HEADER
#define APP_TARGET_CONFIG_HEADER "targets/stm32h743_lxb.h"
#endif
#include APP_TARGET_CONFIG_HEADER
'''))
put(C/'targets/stm32h743_lxb.h',header('stm32h743_lxb.h','''#define PLATFORM_MCU_FAMILY_H7 1
#define PLATFORM_HAL_HEADER "stm32h7xx_hal.h"
#define APP_BOARD_PROFILE_HEADER "boards/lxb743zi_p1/board_profile.h"
#define PLATFORM_UART_HW_BACKEND "mcu/stm32h743/uart_backend.inc"
#define PLATFORM_NV_BACKEND "mcu/stm32h743/nv_backend.inc"
'''))
put(A/'platform/platform_hal.h',header('platform_hal.h','''#include "target_config.h"
#if PLATFORM_MCU_FAMILY_H7 && !defined(STM32H743xx)
#error "Selected H743 target requires STM32H743xx; another MCU needs its own target/board/backend."
#endif
#if defined(STM32H743xx) && defined(STM32F103xB)
#error "Multiple MCU targets selected"
#endif
#include PLATFORM_HAL_HEADER
'''))
put(A/'platform/board_config.h',header('board_config.h','''#include "platform_hal.h"
#include APP_BOARD_PROFILE_HEADER
'''))
put(A/'platform/board_devices.h',header('board_devices.h','#include "board_config.h"'))
put(B/'board_config.h',header('legacy_board_config.h','#include "../../board_config.h" /* compatibility entry; no duplicate configuration */'))
put(B/'board_devices.h',header('legacy_board_devices.h','#include "../../board_devices.h" /* compatibility entry */'))
# Flash partition facts no longer live in application feature configuration.
integration=re.sub(r'^#define BOARD_(?:QSPI_TEST_SECTOR|PROFILE_FLASH_[AB]|LIMIT_FLASH_[AB]|LIMIT_FLASH_SECTOR_SIZE)\s+.*\n','',integration,flags=re.M)
integration=integration.replace('#define INTEGRATION_CONFIG_H','#define INTEGRATION_CONFIG_H\n#include "target_config.h"\n#include APP_BOARD_PROFILE_HEADER',1)
put(C/'integration_config.h',integration)
put(C/'input_config.h',header('input_config.h','''/* Input timing/gesture policy, independent of GPIO wiring. */
#define INPUT_KEY_DEBOUNCE_MS 16U
#define INPUT_KEY_LONG_PRESS_MS 300U
#define INPUT_KEY_MULTI_CLICK_MS 350U
#define INPUT_MOTOR_DEBOUNCE_MS 20U
'''))
p=A/'bsp/bsp_key.h';s=p.read_text(encoding='utf8').replace('#include "input_control.h"','#include "input_control.h"\n#include "input_config.h"');s=s.replace('KEY_DEBOUNCE_TIME_MS             16U','KEY_DEBOUNCE_TIME_MS             INPUT_KEY_DEBOUNCE_MS').replace('KEY_LONG_PRESS_TIME_MS           300U','KEY_LONG_PRESS_TIME_MS           INPUT_KEY_LONG_PRESS_MS').replace('KEY_MULTI_CLICK_TIME_MS          350U','KEY_MULTI_CLICK_TIME_MS          INPUT_KEY_MULTI_CLICK_MS')
for i in range(1,7):
 s=re.sub(r'(#define KEY_Task'+str(i)+r'_PULL\s+)BOARD_KEY_PULL',r'\1BOARD_KEY'+str(i)+'_PULL',s)
 s=re.sub(r'(#define KEY_Task'+str(i)+r'_ACTIVE_STATE\s+)BOARD_KEY_ACTIVE_STATE',r'\1BOARD_KEY'+str(i)+'_ACTIVE_STATE',s)
put(p,s)
# Shared GPIO clock adapter; moving a pin changes its clock selection automatically.
put(A/'platform/board_gpio.h',header('board_gpio.h','''#include "platform_hal.h"
void BoardGpio_EnableClock(GPIO_TypeDef *port);
'''))
gpio='#include "board_gpio.h"\nvoid BoardGpio_EnableClock(GPIO_TypeDef *port)\n{\n'
for c in 'ABCDEFGHIJK':gpio+=f'#if defined(GPIO{c})\n if(port==GPIO{c}){{__HAL_RCC_GPIO{c}_CLK_ENABLE();return;}}\n#endif\n'
gpio+=' (void)port;\n}\n';put(A/'platform/board_gpio.c',gpio)
for rel in ['bsp/bsp_key.c','platform/board_led.c']:
 p=A/rel;s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
 if 'bsp_key' in rel:s=s.replace('    BOARD_KEY_CLOCK_ENABLE();','    /* Clock follows each configured physical port. */').replace('        gpio.Pin = key->config.pin;','        BoardGpio_EnableClock(key->config.port);\n        gpio.Pin = key->config.pin;')
 else:s=s.replace('    BOARD_LED_CLOCK_ENABLE();','    /* Each LED may live on a different GPIO port. */').replace('        s_led[i].pattern = BOARD_LED_OFF;','        BoardGpio_EnableClock(s_led[i].port);\n        s_led[i].pattern = BOARD_LED_OFF;')
 put(p,s)
p=A/'platform/board_motor.c';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s
s=s.replace('==GPIO_PIN_RESET;','==BOARD_MOTOR_KEY_ACTIVE_STATE;')
s=s.replace('if(apb1!=100000000U || apb2!=100000000U','if(apb1!=BOARD_EXPECTED_PCLK1_HZ || apb2!=BOARD_EXPECTED_PCLK2_HZ')
s=s.replace('    __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOE_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_GPIOC_CLK_ENABLE();__HAL_RCC_GPIOG_CLK_ENABLE();','''    BoardGpio_EnableClock(BOARD_MOTOR_HALL_A_PORT);BoardGpio_EnableClock(BOARD_MOTOR_HALL_B_PORT);
    BoardGpio_EnableClock(BOARD_MOTOR_KNOB_PORT);BoardGpio_EnableClock(BOARD_MOTOR_KEY_PORT);
#if MOTOR_DRIVER_PROFILE != 0
    BoardGpio_EnableClock(BOARD_MOTOR_PWM_PORT);BoardGpio_EnableClock(BOARD_MOTOR_DIR_PORT);
#endif''')
s=s.replace('(apb1*2U)/20000000U-1U','BOARD_APB1_TIMER_KERNEL_HZ/BOARD_MOTOR_PWM_COUNTER_HZ-1U').replace('s_pwm.Init.Period=999','s_pwm.Init.Period=BOARD_MOTOR_PWM_PERIOD-1U').replace('(apb1*2U)/1000000U-1U','BOARD_APB1_TIMER_KERNEL_HZ/BOARD_MOTOR_TICK_COUNTER_HZ-1U').replace('s_tick.Init.Period=999','s_tick.Init.Period=BOARD_MOTOR_TICK_COUNTER_HZ/BOARD_MOTOR_TICK_HZ-1U').replace('BOARD_MOTOR_TICK_IRQn,5,0','BOARD_MOTOR_TICK_IRQn,BOARD_MOTOR_TICK_PRIORITY,0')
put(p,s)
p=A/'modules/motor/motor_service.c';s=p.read_text(encoding='utf8').replace('#include "input_control.h"','#include "input_control.h"\n#include "input_config.h"').replace('s_debounce<20U','s_debounce<INPUT_MOTOR_DEBOUNCE_MS').replace('s_debounce==20U','s_debounce==INPUT_MOTOR_DEBOUNCE_MS');put(p,s)
p=A/'platform/board_peripherals.c';s=p.read_text(encoding='utf8');s='#include "board_gpio.h"\n'+s;s=s.replace('__HAL_RCC_TIM4_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE();','BOARD_PWM_CLOCK_ENABLE(); BoardGpio_EnableClock(BOARD_PWM_PORT);').replace('HAL_RCC_GetPCLK1Freq() != 100000000U','HAL_RCC_GetPCLK1Freq() != BOARD_EXPECTED_PCLK1_HZ').replace('s_pwm.Instance = TIM4','s_pwm.Instance = BOARD_PWM_INSTANCE').replace('BOARD_PWM_KERNEL_HZ / 1000000U','BOARD_PWM_KERNEL_HZ / BOARD_PWM_COUNTER_HZ').replace('s_pwm.Init.Period = 20000U - 1U','s_pwm.Init.Period = BOARD_PWM_PERIOD_US - 1U');put(p,s)
# Register two small port implementations once for every IAR configuration.
p=H/'project/EWARM/H743_VisionGripper.ewp';s=p.read_text(encoding='utf8');needle='<name>$PROJ_DIR$/../App/platform/board_resources.c</name>';assert needle in s
pos=s.index('</file>',s.index(needle))+len('</file>');s=s[:pos]+'''\n    <file><name>$PROJ_DIR$/../App/platform/board_gpio.c</name></file>
    <file><name>$PROJ_DIR$/../App/platform/board_nv_storage.c</name></file>'''+s[pos:];put(p,s)
print('Target selector, six board groups, input/timer/GPIO configuration created')
