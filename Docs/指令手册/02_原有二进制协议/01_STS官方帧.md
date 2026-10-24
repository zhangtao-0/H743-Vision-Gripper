# 舵机官方协议：HEX 字节，不是文本

当前原协议栈保留上游帧结构：`FF FF ID Length Instruction Params CheckSum`，总长=`Length+4`，`Length=参数字节数+2`，校验=`~sum(ID到最后参数)&0xFF`。数据低字节在前；状态包第5字节是Error，不是请求Instruction。

此页所称“官方”表示飞特设备协议类别，不表示任何帧都允许无条件透传。当前命令层会检查写地址/范围/危险操作；写入可能被拒绝。实际转发路径见[ForwardFrame与ValidateWriteFrame](../../../project/App/drivers/Servo/feetech_sts_command.c)。

## 可复制示例

以下全是实际HEX。当前默认配置ID是0，表中优先给0；ID1另列用于说明改ID必须重算校验。发送到HOST USART1，而非USB CDC二进制回显接口。原command栈必须已经启动。

| 功能 | HEX | 说明 |
|---|---|---|
| PING ID0 | `FF FF 00 02 01 FC` | 查询0；应答示例`FF FF 00 02 00 FD`，非广播 |
| PING ID1 | `FF FF 01 02 01 FB` | 查询1；不能因为旧例为1就假设实物ID=1 |
| 读ID0位置 | `FF FF 00 04 02 38 02 BF` | 地址0x38、2字节 |
| 读ID1位置 | `FF FF 01 04 02 38 02 BE` | 同上，ID不同 |
| 读ID0综合反馈 | `FF FF 00 04 02 38 0F B2` | 从0x38读15字节；字段按STS寄存器表解析 |
| ID1扭矩上力 | `FF FF 01 04 03 28 01 CE` | 写0x28=1，可能使机构保持/动作 |
| ID1扭矩卸力 | `FF FF 01 04 03 28 00 CF` | 写0x28=0，不等于“保持位置停止” |
| ID1位置模式 | `FF FF 01 04 03 21 00 D6` | 旧表D7校验错误，应为D6 |
| ID1位置2048 | `FF FF 01 09 03 2A 00 08 00 00 E8 03 D5` | 从0x2A写位置、时间0、速度1000；会动作，受写校验策略 |
| ID1加速度+位置 | `FF FF 01 0A 03 29 00 00 08 00 00 E8 03 D5` | 从0x29写加速度0、位置2048、时间0、速度1000 |
| 广播ACTION | `FF FF FE 02 05 FA` | 执行已缓存REG_WRITE，不是通用“启动全部设备” |

位置2048是编码位置单位，不能直接写成ASCII“2048”，也不能按PWM的90度换算。角度转换以当前STS实现和机械限位为准。改变ID或字段后用[离线工具](../../../Tools/command_frame.py)重算校验，不手改一个字节保留旧尾码。

## 常见指令码

PING=01、READ=02、WRITE=03、REG_WRITE=04、ACTION=05、RECOVERY=06、RESET=0A、SYNC_READ=82、SYNC_WRITE=83，具体支持仍看当前常量和校验策略。恢复/复位/写EEPROM可能改变设备配置；这里不把它们作为首次连线排查步骤。

查询通常等待应答；当前WRITE类默认不等待状态包，文本“已发送”或HAL_OK只表示传输路径接受，不证明舵机已执行或位置到达。前置供电、TTL端口、ID、总线速率仍须验证。

## 与 `@servo probe` 的区别

设备未在线时先用 `@servo probe 0 1000000` 和 `@status`查看只读探测。原命令层尚未启动时，直接往HOST发官方PING帧不会自动越过启动条件来建立在线状态。不要为尝试唤醒而连续灌入动作帧。

这里依据当前[总线协议常量](../../../project/App/drivers/Servo/feetech_sts_bus.h)和命令层；上游固定版本链接见[对照记录](../04_维护与证据/01_上游对照与修订记录.md)。
