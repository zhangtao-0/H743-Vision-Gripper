# 配置入口与职责

[完整配置导航](../../../Docs/解耦与移植/README.md)。

target_config.h只选择目标头；targets/stm32h743_lxb.h指定SDK、板型和芯片后端。板型的六组硬件事实放在platform/boards/<板型>/，不要继续在业务.c或Core中新增引脚常量。

motor_config是电机设备/控制参数；input_config是输入时序策略；integration_config是功能组合与相机格式；feature_config是夹爪/通信业务策略；feetech_sts_register是器件寄存器资料。运行期可调值通过UiPort/上位机修改，保存到Flash的配置不是硬件引脚表。
