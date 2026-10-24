# RTC、存储与 USB 怎样与主循环协作

## 存储链不是单层

应用命令 → BoardStorage_FileTest → FatFs文件API → diskio → SD BSP → HAL SDMMC → 卡。`sd_ready`证明的层次比 `fs_ready`低，文件读回验证又比“挂载成功”更进一步。

当前diskio使用对齐的512字节bounce缓冲，SD为阻塞路径，不是DMA零拷贝。学习时去 [board_storage](../../../project/App/platform/board_storage.c)逐步核对参数单位：文件字节数、扇区号、块数和内存指针不要混写。

QSPI则是独立器件/命令链，进入0x90000000映射前后做只读比较，擦写前退出映射。内部Flash的夹爪记录又是另一条路径，不经过FatFs或W25Q64。

## 日历与持久化

RTC提供日历，可为文件系统时间戳服务；初始化有默认时间和备份标记。夹爪开合校准用内部Flash双槽、CRC与序号，不靠RTC决定哪份记录更新。

这说明“共享时间/存储资源”不等于“所有数据都放同一个文件”。每类数据应按频率、掉电需求和更新粒度选择媒介。

## USB命令路径

USB IRQ经PCD与Device库进入CDC_Receive_FS，再进入UsbConsole_OnReceive。命令缓存在控制台输入槽，主循环执行。日志写入USB发送队列，端点忙时等待后续Task；TX缓冲不会在传输完成前被覆盖。

UART HOST和USB共用命令解释器，但各有输入上下文、不同底层时序。设备枚举后出现的COM号码只是主机侧标识。

## 哪些操作不是普通只读状态

`@status`读状态；`@sd read`读块；`@sd filetest`创建专属文件；`@qspi test`擦最后4KiB专用区；RTC set改时间。不要把它们当成同一种“测试按钮”。

涉及慢操作时，控制台维护门检查STS运动/控制状态及电机使能状态。学习过程中先读清副作用，再执行命令。

阅读入口：[Execute](../../../project/App/modules/integration/integration_console.c)、[USB队列](../../../project/App/platform/usb_console.c)、[内部Flash记录](../../../project/App/platform/claw_limit_storage_h7.c)。
