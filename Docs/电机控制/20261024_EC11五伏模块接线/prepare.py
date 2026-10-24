from pathlib import Path
import shutil,json
H=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent
for rel in ['project/App/platform/board_motor.c','project/App/platform/boards/lxb743zi_p1/board_devices.h']:
 p=H/rel;shutil.copy2(p,E/(p.name+'.before'))
(E/'方案.md').write_text('''# EC11五伏模块接入方案

用户确认模块VCC=5V、C为按压。沿用A=PG2/U10-57、B=PG3/U10-58、C=PC13/U10-7；不新增GPIO占用。ST STM32H743ZI数据手册确认PC13=FT、PG2/PG3=FT_h；承受高于4V输入时，内部上下拉必须禁用。因此将三路旋钮GPIO改为NOPULL，依赖模块既有上拉/驱动，未带上拉的开关输出另需外部上拉。禁止把C当公共端接地。

源码将旋转输入和按压输入的Pull分别收敛到board_devices.h。5V模块信号接入后，三脚保持数字输入模式。现有业务无须增加：Motor keys授权同时控制旋转调速和KEY7按压，启停/长按换向已实现，KEY1～4不变。默认上电授权关闭。

验证：电机服务主机回归、所有电机驱动profile编译、引脚表和教学文档校验、主配置IAR构建；若ST-Link可用，则先停机再烧录校验。此次不自动启动电机。实体旋钮转动及C低有效按压待用户接线后验证，不能用构建通过代替实测。

来源：项目.firecrawl/20261024-stm32h743-pa8-af.md中引脚表及5V容限条件；原文https://www.st.com/resource/en/datasheet/stm32h743zi.pdf 。
''',encoding='utf8')
p=H/'project/App/platform/boards/lxb743zi_p1/board_devices.h';s=p.read_text(encoding='utf8');s=s.replace('#define BOARD_MOTOR_KNOB_B GPIO_PIN_3','#define BOARD_MOTOR_KNOB_B GPIO_PIN_3\n#define BOARD_MOTOR_KNOB_PULL GPIO_NOPULL /* PG2/PG3 FT inputs; module supplies5V pull-ups */');s=s.replace('#define BOARD_MOTOR_KEY_PIN GPIO_PIN_13','#define BOARD_MOTOR_KEY_PIN GPIO_PIN_13\n#define BOARD_MOTOR_KEY_PULL GPIO_NOPULL /* PC13 FT input; no internal pulls above4V */');p.write_text(s,encoding='utf8')
p=H/'project/App/platform/board_motor.c';s=p.read_text(encoding='utf8');s=s.replace('g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_PULLUP;HAL_GPIO_Init(BOARD_MOTOR_KNOB_PORT,&g);','g.Mode=GPIO_MODE_INPUT;g.Pull=BOARD_MOTOR_KNOB_PULL;HAL_GPIO_Init(BOARD_MOTOR_KNOB_PORT,&g);');s=s.replace('g.Pin=BOARD_MOTOR_KEY_PIN;HAL_GPIO_Init(BOARD_MOTOR_KEY_PORT,&g);','g.Pin=BOARD_MOTOR_KEY_PIN;g.Pull=BOARD_MOTOR_KEY_PULL;HAL_GPIO_Init(BOARD_MOTOR_KEY_PORT,&g);');p.write_text(s,encoding='utf8')
print('Plan and two GPIO configuration edits saved')
