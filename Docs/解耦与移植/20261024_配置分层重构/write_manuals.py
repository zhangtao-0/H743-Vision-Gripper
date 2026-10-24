from pathlib import Path
import csv,json,re
H=Path(__file__).resolve().parents[3];D=H/'Docs/解耦与移植';E=Path(__file__).resolve().parent;P=H/'project';B=P/'App/platform/boards/lxb743zi_p1'
def put(p,s):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(s,encoding='utf8')
put(D/'README.md','''# 配置分层、解耦与移植总入口

当前源码已实施2026-10-24配置分层重构。板级事实由一份目标选择和六组配置维护；旧board_config/board_devices仅保留兼容入口。业务继续使用逻辑按键和设备角色，不绑定具体串口号。

| 要做的事 | 唯一主要配置位置 |
|---|---|
| 选择开发板/SDK/芯片后端 | [target_config.h](../../project/App/config/target_config.h) → [当前目标](../../project/App/config/targets/stm32h743_lxb.h) |
| 修改KEY1～6引脚、逐键电平/上下拉，LED引脚/极性 | [board_io_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_io_cfg.h) |
| 修改HOST/STS/蓝牙UART、波特率、TX/RX/AF/IRQ/时钟，以及原生USB资源 | [board_serial_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_serial_cfg.h) |
| 电机驱动、霍尔、EC11/KEY7、PWM舵机的硬件资源 | [board_motion_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_motion_cfg.h) |
| 屏幕SPI/控制线、相机DCMI/SCCB/DMA/IRQ | [board_vision_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_vision_cfg.h) |
| SD/QSPI资源、内部Flash配置/限位的分区 | [board_storage_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_storage_cfg.h) |
| 主时钟PLL、APB/定时器频率、板级能力 | [board_system_cfg.h](../../project/App/platform/boards/lxb743zi_p1/board_system_cfg.h) |
| 调整电机PID、CPR、限速、旋钮每格速度 | [motor_config.h](../../project/App/config/motor_config.h)，不是硬件引脚表 |
| 输入消抖/手势时长 | [input_config.h](../../project/App/config/input_config.h) |
| 功能开关、相机类型/尺寸 | [integration_config.h](../../project/App/config/integration_config.h) |
| 夹爪策略、打印、蓝牙帧/队列默认值 | [feature_config.h](../../project/App/config/feature_config.h) |

六组硬件配置由[board_profile.h](../../project/App/platform/boards/lxb743zi_p1/board_profile.h)汇总。外设引脚和电平属于板型；蓝牙型号/透传帧、PID等属于设备/业务。不会把一次运行中调好的值反写成硬件引脚配置。

- [配置读取链路与工程目录](01_配置链路与目录.md)
- [换引脚、换UART、换蓝牙、换芯片的步骤](02_换板换芯片操作.md)
- [功能、适配层与移植边界矩阵](03_功能接口与边界.csv)
- [配置宏索引CSV](配置宏索引.csv)：值宏的原始值、展开值与来源；函数式时钟宏请直接看对应头文件。
- [本轮方案、验证与变更证据](20261024_配置分层重构/README.md)
- [当前按设备接线表](../引脚与接线/00_按外设接线.md)

同一芯片家族修改引脚/串口时，主要改对应配置组，初始化/MSP/IRQ随表生成。跨芯片家族必须提供匹配SDK、启动/中断文件、链接脚本以及芯片后端；六组配置不能创造不存在的DCMI、DMA、USB或内存。当前只验证H743构建，没有宣称F103、ESP32-C3已完成适配。

本轮未烧录、未操作运行串口，也未启动电机/舵机。设备上仍是上一版EC11五伏输入固件；本轮构建与实物证据分开存放。
''')
put(D/'01_配置链路与目录.md','''# 配置读取链路与工程目录

```text
编译选项APP_TARGET_CONFIG_HEADER（可省略，默认当前H743）
  → App/config/target_config.h
    → App/config/targets/stm32h743_lxb.h
      ├─ PLATFORM_HAL_HEADER → platform_hal.h → 当前SDK头
      ├─ APP_BOARD_PROFILE_HEADER → board_profile.h → 六组硬件配置
      ├─ PLATFORM_UART_HW_BACKEND → H743 UART硬件后端
      └─ PLATFORM_NV_BACKEND → H743内部Flash后端
```

原有#include "board_config.h"继续有效，优先使用platform/board_config.h；原板型目录内同名文件只是转发。board_devices.h也只转发，实际宏定义只在六组配置内，不能在兼容入口再复制一套。

## 为什么按这六组拆

io把通用人机输入和LED放一起，数量少且同样依赖GPIO；motion把霍尔反馈、EC11、驱动方向/PWM和PWM舵机放一起，避免一台电机拆散到四处；serial把所有通信角色及原生USB资源放一起，保留UART和USB不同的传输属性；vision负责屏和相机；storage负责SD/QSPI与内部Flash分区；system负责时钟事实和能力。不是每个引脚一个文件，也不是所有HAL细节塞进一个上千行总配置。

## UART配置表如何真正生效

board_serial_cfg.h的BOARD_UART_BINDINGS将SERVO/HOST/BLUETOOTH映射到句柄名称。每个角色旁边集中写Instance、baud、TX/RX端口和引脚、AF、电阻、速度、IRQ、优先级、时钟源与时钟开关。

- Core/main.c用表声明句柄，调用BoardUart_ConfigureHardware，不再维护三份MX_USARTx初始化。
- platform/board_resources.c从表生成角色绑定。
- platform/mcu/stm32h743/uart_backend.inc从表生成配置描述，完成HAL初始化、GPIO时钟、MSP和NVIC。
- Core/MSP转发到适配层；Core/IRQ用表生成具体USART中断函数，并按角色分发。
- uart_router.c分配收发事件；VOFA、蓝牙通过BoardUart角色接口收发，不直接启动HAL_UART_Transmit_IT/Receive_IT。

硬件初始化前检查重复UART实例及IRQ；不能只修改句柄表而漏掉另外三处。SDK特有的FIFO、RCC时钟组选项只在H743后端。board_uart.h本身不引入HAL类型；旧协议对外回调/返回类型仍有HAL兼容ABI，尚未宣称整个协议栈完全脱离STM32 HAL。

## GPIO与定时器

KEY1～6的port、pin、逐键pull和active_state在io组，BSP按表扫描；PC13的KEY7和旋钮A/B在motion组。两套物理/模拟输入仍走同一去抖/手势/授权逻辑，逻辑ID、旧二进制命令、屏幕顺序不因换物理引脚而改变。

BoardGpio_EnableClock按实际配置端口开启时钟，按键、LED、电机、屏幕、相机、QSPI、SD、USB不需要另改一份GPIO时钟清单。主程序的安全启动电平也由设备配置产生，保留NSS/背光/LED的原有初值。

电机PWM输出API仍是±1000千分比，适配层将其换算到定时器周期；不能把ARR计数直接当通用占空比。服务接口固定1ms，PWM舵机接口固定微秒，本轮为不兼容的计数频率增加编译检查，避免修改配置后时间单位悄悄出错。更换定时器还要确认通道、AF和APB时钟，不能只改引脚。

## 内部Flash

BoardNv_Read/BoardNv_Rewrite使用PROFILE_A/B、LIMIT_A/B逻辑区域，不向UI暴露FLASH_EraseInitTypeDef、缓存或物理地址。UI负责160字节v1记录、CRC、序号及双槽选择；限位负责32字节v1记录。H743后端负责32字节编程、缓存维护、擦除、Bank交换拒绝、分区重叠/越界拒绝和写后校验。

本轮保持Bank2扇区4/5为开机配置、6/7为机械限位，地址和记录版本不变。芯片后端只允许预留区，不能靠修改一个地址宏把它指向程序区；改Flash布局必须同步链接脚本和后端保护规则。新芯片可以用片内Flash、EEPROM或其他存储实现同一接口，但仍要保证单槽失败不破坏另一有效槽。
''')
put(D/'02_换板换芯片操作.md',r'''# 换引脚、换设备与换芯片操作

## 一：同一块H743改按键

1. 查[空闲与预留表](../引脚与接线/04_空闲与预留资源.md)以及原理图和现有接线。
2. 改board_io_cfg.h对应BOARD_KEYn_PORT/PIN；按电路设置该键的BOARD_KEYn_PULL、BOARD_KEYn_ACTIVE_STATE。无需修改BSP扫描算法，也无需手动添加另一处GPIO时钟。
3. EC11的A/B/C则改board_motion_cfg.h，保留输入电平约束；当前5V接法依赖已核对的FT脚和NOPULL，换脚必须重新核实5V容限。
4. 同步引脚资源JSON与来源哈希，经人工审查后生成表格，运行verify_pin_reference.py。生成器不会自动把未核实新脚标成合法。
5. 构建后验证实体和@key模拟、消抖、单/双击、授权及屏幕顺序。换GPIO不改变逻辑KEY编号。

## 二：把HOST或蓝牙换到另一UART

只在board_serial_cfg.h中完整修改该角色的Instance、TX/RX、AF、IRQ/Handler、RCC选择/分组和时钟开关。波特率也在同一文件。句柄名称可以保留，huart1这个变量名不等于必须使用USART1。

初始化、MSP和IRQ都会读取同一配置，不手改三处Core函数。仍必须确认新UART是芯片真实资源、AF合法、引脚可引出且不与其他设备冲突。本轮主机测试通过交换HOST/STS的完整配置证明表会联动，但这不是当前实物建议改线。

串口帧格式目前公共配置为8N1；要使用不同格式，需扩展描述表为逐角色格式，不能误以为仅改变波特率就等价。UART1/6与UART2/3/4/5/7/8在H7使用不同RCC时钟组，必须配对正确。

## 三：换蓝牙模块

保留BLUETOOTH角色，按模块实际设置更改波特率、接线和电平；保持协议帧或有意修改feature_config/蓝牙协议层。JDY-31与HC-08的型号、无线能力、AT命令、手机端支持等另查设备资料，串口透传接口相同不代表无线特性相同。不能通过UART宏自动完成模块固件配置或配对。

## 四：新增板型（同芯片）

1. 复制boards/lxb743zi_p1为新板目录，修改六组配置和board_profile汇总，原板不覆盖。
2. 新建config/targets/my_board.h，按当前目标提供PLATFORM_HAL_HEADER、APP_BOARD_PROFILE_HEADER和芯片后端路径。相同H743可复用对应H7后端。
3. IAR预处理器定义APP_TARGET_CONFIG_HEADER="targets/my_board.h"，保持正确MCU型号宏/SDK/启动和链接设置。
4. 检查时钟输入、PLL/APB/TIM核频率、Flash容量、DMA可访问内存和外设能力；记录新板引脚表。
5. 调整Tools/configuration_facts.py默认事实入口/引脚资源库，或为新板增加独立账本。现有脚本专门核对当前鹿小班原理图，不能把它通过当成新板已核实。

## 五：跨芯片家族

配置文件是入口，不是另一芯片的SDK实现。以F103为例，必须替换：对应HAL/CMSIS/startup、MCU宏、编译选项及链接脚本；UART硬件后端；GPIO/TIM/DMA实现；片内Flash后端；USB设备层；需要的屏幕/摄像头/SD/QSPI底层。算法、逻辑按键、记录格式、命令语义与角色接口尽量复用。

当前H7的TIMPRE、FIFO、Flashword、Cache、DCMI和DMA缓冲段不能直接搬到F1。没有DCMI等能力时关闭对应功能并调整源文件注册，或者提供另一种外部硬件传输后端；关闭一个功能宏并不会让编译器中的H7 SDK类型自动变成F1类型。能力宏会明确阻止在缺少DCMI/USB/SDMMC/QSPI声明的板型上开启相应功能。

不要继续把旧platform_hal.h曾有的F103头文件分支理解为F103工程已适配完成。现在默认目标会明确拒绝错误MCU宏；其他芯片尚未交付可运行目标配置。

## 每次移植的最小验证

先构建与配置约束检查；再验证只读状态/串口收发、GPIO初始态、输入电平、设备ID和反馈；最后才验证输出/运动。先保留旧配置与校准记录，不通过格式未变之外的推断声称保存值一定可跨设备使用。

当前回归命令（在H743根目录，用已有Python环境运行）：

```powershell
$py = '.\Host\vision_gripper\.venv\Scripts\python.exe'
& $py .\Tools\run_portability_tests.py
& $py .\Tools\run_p2_host_tests.py
& $py .\Tools\run_p3_host_tests.py
& $py .\Tools\run_motor_tests.py
& $py .\Tools\run_motor_plot_tests.py
& $py .\Tools\run_ui_tests.py
& $py .\Tools\run_integration_host_tests.py
& $py .\Tools\run_camera_regression.py
& $py .\Tools\verify_pin_reference.py
```

IAR三配置按顺序构建；同一个工程的构建工具会清理共享状态，不能把三个配置并行启动。本轮保留了并发清理失败的日志，改为顺序构建后通过。
''')
rows=[
 ['KEY1～6/LED','board_io_cfg.h','bsp_key.c / board_led.c / board_gpio.c','Key_* / Input_* / BoardLed_*','逐键GPIO/有效电平/上拉；逻辑ID不变','CMSIS锁与HAL时间/GPIO由目标平台提供'],
 ['UART与蓝牙','board_serial_cfg.h','board_resources.c / board_uart.c / mcu/stm32h743/uart_backend.inc','BoardUart_* / Bluetooth_*','角色表生成初始化/MSP/IRQ；蓝牙业务帧单独配置','其他MCU需UART后端；旧回调/状态类型仍保留HAL兼容ABI'],
 ['USB CDC','board_serial_cfg.h','USB_DEVICE/Target/usbd_conf.c / usb_console.c','UsbConsole_*','USB设备句柄/协议栈属于目标SDK适配','HAL PCD/USB描述符、中断与DMA内存另配'],
 ['MG310/EC11','board_motion_cfg.h + motor_config.h','board_motor.c / board_gpio.c','MotorService_* / MotorControl_*','算法与GPIO/TIM隔离；反馈/旋钮是不同输入','TIMPRE/编码器/IRQ当前H743；1ms调度固定契约'],
 ['PWM舵机','board_motion_cfg.h','board_peripherals.c / bsp/tim.c','BoardPwm_* / PwmServoControl_*','TIM实例、引脚、AF和核频率归配置','当前1MHz微秒计数器；不得另改为电机20kHz时基'],
 ['STS总线舵机','board_serial_cfg.h + feature_config.h','feetech_sts_port.c / board_uart.c','FeetechSts* / ServoP3_*','SERVO角色不绑定huart2实例号','线路电平/转接板方向、帧时序、HAL兼容返回类型需核实'],
 ['LCD','board_vision_cfg.h','vendor/lcd / board_peripherals.c','LCD_* / UiService_*','SPI实例/AF/GPIO/时钟选择集中','换控制器需新驱动，非只换引脚'],
 ['摄像头','board_vision_cfg.h + integration_config.h','vendor/camera / board_camera.c','BoardCamera_*','DCMI/SCCB/IRQ/DMA资源集中','当前H743 DCMI后端、IAR.camera_dma缓冲段，不是通用摄像头HAL'],
 ['SD/QSPI','board_storage_cfg.h','vendor/sd / vendor/qspi / board_storage.c','BoardStorage_*','资源与时钟选择集中','SDMMC/QSPI/Flash命令驱动仍依赖目标SDK与器件'],
 ['开机配置/限位','board_storage_cfg.h','board_nv_storage.c / mcu/stm32h743/nv_backend.inc','BoardNv_* / UiProfileStore_* / ClawLimitStorage_*','业务记录格式与物理擦写分离','当前只允许2MiB H743预留扇区；布局变化同步链接脚本与保护'],
 ['时钟/RTC/MPU','board_system_cfg.h','Core/main.c / board_peripherals.c','SystemClock_Config / BoardRtc_* / BoardMemory_*','PLL/APB/定时器时钟事实与能力归配置','供电模式、RTC/MPU/Cache/DMA内存属于芯片后端；跨系列替换Core/SDK']
]
with (D/'03_功能接口与边界.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['功能','主要配置','适配实现','上层接口','本轮解耦方式','仍需适配的硬件边界']);w.writerows(rows)
put(P/'App/config/README.md','''# 配置入口与职责

[完整配置导航](../../../Docs/解耦与移植/README.md)。

target_config.h只选择目标头；targets/stm32h743_lxb.h指定SDK、板型和芯片后端。板型的六组硬件事实放在platform/boards/<板型>/，不要继续在业务.c或Core中新增引脚常量。

motor_config是电机设备/控制参数；input_config是输入时序策略；integration_config是功能组合与相机格式；feature_config是夹爪/通信业务策略；feetech_sts_register是器件寄存器资料。运行期可调值通过UiPort/上位机修改，保存到Flash的配置不是硬件引脚表。
''')
put(H/'Docs/教学手册/01_启动与配置/02_配置资源与编译目标.md','''# 引脚、功能和工程配置分别在哪一层

2026-10-24配置分层重构后，硬件配置按六组维护。完整[配置导航](../../解耦与移植/README.md)、[换板换芯片教程](../../解耦与移植/02_换板换芯片操作.md)。

| 文件 | 负责 |
|---|---|
| target_config.h / targets/stm32h743_lxb.h | 目标选择、SDK入口、板型和芯片后端 |
| board_profile.h | 汇总六组硬件事实 |
| board_io_cfg.h | KEY1～6、LED GPIO/极性/逐键上拉 |
| board_serial_cfg.h | UART角色/实例/速率/引脚/AF/时钟/IRQ，以及原生USB |
| board_motion_cfg.h | MG310、霍尔、EC11/KEY7、PWM舵机 |
| board_vision_cfg.h | LCD和相机GPIO/SPI/DCMI/DMA/时钟/IRQ |
| board_storage_cfg.h | SD/QSPI资源和内部Flash分区 |
| board_system_cfg.h | PLL/APB/定时器频率与能力 |
| motor_config / input_config / integration_config / feature_config | 控制参数、输入手势、功能组合、业务策略 |

旧board_config/board_devices仍能被包含，但只是兼容入口。board_resources.c读取角色表，不再单独维护第二份绑定。Core初始化/MSP/IRQ从适配入口/表读取，而不是靠修改一个句柄指针来假设其他三处都已修改。

当前仍是USART2 PA2/PA3给STS，USART1 PA9/PA10给HOST，USART3 PB10/PB11给蓝牙；霍尔A=PA8、B=PE11；L298N ENA=PB14、IN1/2=PE10/12；EC11 PG2/PG3、按压PC13。当前旋钮三路和霍尔两路5V输入使用已核对FT数字脚、内部NOPULL；换芯片后必须重新核实，不能把这个条件复制成所有GPIO都能接5V。

IAR三个配置仍为H743_VisionGripper、H743_VisionGripper_OV2640、H743_VisionGripper_NoCamera。本轮构建不代表另一芯片已移植或实物已烧录。当前.ioc只是历史生成基线，重新生成后必须保留角色入口、IRQ和App适配注册。
''')
p=H/'Docs/教学手册/02_数据与控制链路/01_UART与STS的完整旅程.md';s=p.read_text(encoding='utf8');old='[main](../../../project/Core/Src/main.c)定义并初始化huart1/2/3；[MSP](../../../project/Core/Src/stm32h7xx_hal_msp.c)给UART选择时钟、配置GPIO和NVIC；[角色表](../../../project/App/platform/board_resources.c)给SERVO选择huart2。业务不知道PA3，但平台必须配置正确。';new='[串口配置组](../../../project/App/platform/boards/lxb743zi_p1/board_serial_cfg.h)定义角色与完整硬件资源；[main](../../../project/Core/Src/main.c)按表声明句柄并调用BoardUart_ConfigureHardware；[MSP](../../../project/Core/Src/stm32h7xx_hal_msp.c)只转发，实际GPIO/RCC/NVIC由[H743后端](../../../project/App/platform/mcu/stm32h743/uart_backend.inc)按同一表配置。SERVO当前仍映射huart2/USART2，但更换角色配置不用在业务中搜索PA3。';assert old in s;s=s.replace(old,new);put(p,s)
p=H/'Docs/教学手册/00_项目导读/02_目录分层与职责.md';s=p.read_text(encoding='utf8').replace('板型下的board_devices保存GPIO/TIM','板型下的board_profile汇总六组GPIO/外设/时钟配置，旧board_devices只保留兼容转发');put(p,s)
# Register added teaching documents for the existing library link validator.
p=H/'Docs/教学手册/05_维护与证据/手册清单.json';reg=json.loads(p.read_text(encoding='utf-8-sig'));R=H.parents[3]
for f in [D/'README.md',D/'01_配置链路与目录.md',D/'02_换板换芯片操作.md']:
 reg['p:解耦与移植/'+f.name]={'path':f.relative_to(R).as_posix(),'kind':'p','title':f.read_text(encoding='utf8').splitlines()[0][2:]}
p.write_text(json.dumps(reg,ensure_ascii=False,indent=2),encoding='utf8')
print('Configuration guide, migration recipes, API matrix and current teaching guide updated')
