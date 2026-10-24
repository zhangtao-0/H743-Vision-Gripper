# 配置分层、解耦与移植总入口

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
