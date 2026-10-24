# 从复位到 App_Task：按实际代码追一次

## 第一段：启动与HAL

先读 [启动文件](../../../project/EWARM/startup_stm32h743xx.s)、[SystemInit](../../../project/Core/Src/system_stm32h7xx.c)，再进入 [main](../../../project/Core/Src/main.c)。向量表、中断名、栈和运行库初始化先于应用代码，不由CubeMX在线参与运行。

main当前顺序是基础MPU → HAL_Init → BoardMemory_Configure → SystemClock_Config → MX_GPIO_Init与USART1/2/3 → App_Init → while调用App_Task。MX函数只是C代码，手动维护同样可以运行。

## 第二段：App_Init

[App_Init](../../../project/App/app_main.c)先初始化灯和四键，再检查UART资源；可用时建立HOST协议，输出启动文本，进入Integration_Init，然后启动P3只读探测。最后记录启动时刻，控制层尚未因上电直接执行运动目标。

注意 `BoardUart_InitAll` 名字虽带Init，这里主要做角色/句柄/速率一致性校验；UART硬件初始化此前已由main完成。读函数体比猜名字可靠。

## 第三段：Integration_Init

[整合初始化](../../../project/App/modules/integration/integration_service.c)配置额外时钟，然后依次准备屏幕、RTC、存储、USB、相机、蓝牙、PWM服务、电机服务，记录初始化耗时与状态。

部分器件不存在时会记录失败状态；不是每个外设返回失败都会终止整个系统。另一方面，某个同步初始化耗时过长也会推迟后续步骤，学习时应记录耗时而非假设每个Init都是瞬间完成。

## 主循环与中断分别做什么

App_Task处理四键、HOST任务、P3/控制业务、指示灯和Integration_Task。Integration_Task再调用USB/蓝牙/相机等协作任务，并周期更新状态。

TIM6中断独立为电机采样/控制计时，不靠App_Task执行次数。USART/DCMI/DMA IRQ在另一路异步产生数据，Task再消费状态。一个应用中因此同时存在协作调度与中断驱动。

## 学习练习

在纸上列出Init调用者、Task调用者与IRQ调用者三列。解释为什么“已执行HAL_UART_Init”还不代表P3收到应答，以及为什么主循环能运行不等于TIM6一定已被正确接到向量。

文档阅读不需要修改当前运行固件。若做断点实验，记录暂停会如何影响串口接收和控制时基，不能把调试器暂停造成的超时当成原设备故障。
