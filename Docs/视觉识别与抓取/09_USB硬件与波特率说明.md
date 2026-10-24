# 从原理图确认USB虚拟串口及波特率含义

核查日期2026-10-23。结论：鹿小班LXB743ZI-P1核心板的Type-C具备直接接入H743的USB数据线路，可通过当前固件实现USB CDC虚拟串口。原理图证明连线条件，源代码证明实现方式；没有在本轮重新插线、枚举或做新版实物传图测试。

## 原理图上实际接到了哪里

[开发板原理图](../../../../../../01_嵌入式技术资料库/01_芯片与内核资料/09_硬件信息/芯路城_鹿小班_STM32H743ZIT6/01_原理图/LXB743ZI-P1原理图.pdf)，第1页“USB接口电路”及U1主控引脚；查看[接口局部图](20261023_USB原理图核查/USB接口电路.png)与[主控引脚局部图](20261023_USB原理图核查/主控USB引脚.png)。

| U11 Type-C | 网络 | U1 STM32H743ZIT6 |
|---|---|---|
| A6/B6 DP | USB0_P | PA12，封装104脚，USB D+ |
| A7/B7 DN | USB0_N | PA11，封装103脚，USB D− |
| CC1/CC2 | R12/R13，各5.1kΩ下拉到地 | Type-C设备侧识别，非串口TX/RX |
| VBUS/GND | +5V/GND | 供电与公共参考 |

图纸注释指出核心板没有硬件USB转串口电路。这指没有CH340/CP2102之类的桥接芯片，并不否定H743自身的USB外设。PA9/PA10的USART1则从H1等引出，是另一条独立线路。裸USB数据线没有直接转换成UART信号。

## 固件怎样变成Windows里的COM口

[usb_device.c](../../project/USB_DEVICE/App/usb_device.c)注册USBD_CDC类与接口；[usbd_conf.c](../../project/USB_DEVICE/Target/usbd_conf.c)将PA11/12配置为AF10，使用USB_OTG_FS、内置PHY、PCD_SPEED_FULL与HSI48时钟。[usbd_cdc_if.c](../../project/USB_DEVICE/App/usbd_cdc_if.c)把USB接收交给当前控制台，发送经CDC端点。

```text
电脑程序 → Windows COM驱动 → USB CDC数据包 → Type-C D+/D− → H743 USB外设 → 应用
```

只有运行具备CDC设备功能的固件并成功枚举后，电脑才会出现对应COM口；仅接电源、进入DFU、数据线没有数据芯或固件未运行CDC时，不能推断一定有COM。DFU下载设备也不是同一个CDC应用接口。

## 为什么程序填写115200仍可传图

当前CDC_Control_FS的CDC_SET_LINE_CODING分支没有更改UART、USB时钟或发送节拍；GET_LINE_CODING固定返回115200、8N1。viewer.py中serial.Serial(port,115200,...)是使用COM API打开设备时提供的串口兼容设置，当前固件不按115200限制USB有效负载速率。将上位机设置改为另一个被驱动接受的波特率，不会让当前USB链路相应变速。

| 通道 | 115200的含义 | 是否要两端匹配 |
|---|---|---|
| USB-TTL→USART1 PA9/PA10 | 真实UART比特时序，8N1约11520有效字节/秒 | 要匹配 |
| 原生USB CDC→PA11/PA12 | 当前实现只作COM接口兼容参数 | 当前固件未用它控制传输速度，统一选115200方便操作 |

如果以后将CDC做成真正的“USB到某个UART桥”，并在SET_LINE_CODING里调用UART重配置，波特率才会影响桥后的UART段；那是另一个功能，本版没有实现。

## USB真正的速率与图像帧率

当前为USB Full Speed，总线标称12Mbit/s；依据[STM32H743数据手册](../../../../../../01_嵌入式技术资料库/01_芯片与内核资料/01_数据手册/STM32H743/DS_stm32h743zi.pdf)第52页。不是12MB/s，也不是Type-C就能达到USB3速度。应用有效吞吐还受USB协议、64字节数据端点、主机调度、固件循环、相机采集和缓冲租用影响。

240×240 RGB565原始帧为115200字节，2帧/秒约230400字节/秒，加每帧40字节头。正常115200 UART传一帧单算8N1就约10秒；USB CDC不受这个UART时序限制。但是否达到目标帧率仍需实物测量，不能把12Mbit/s直接当作图像吞吐量。

## 实际怎么使用

1. 烧录具备CDC和图像发送功能的对应FD5640配置，保留原接线。
2. 用有数据线芯的Type-C线连接核心板U11与电脑，在设备管理器或viewer --list-ports中辨认新增COM。
3. 该COM由图像viewer独占，程序仍填写115200即可；图像按自定义二进制帧解码，普通串口助手看到的不是可读文字。
4. VOFA继续使用另一根USB-TTL连接USART1，115200、8N1。两条链路的COM号不同，不要同时打开同一个COM。
5. 检查图像随物体变化，再测实际帧率、CRC与并行命令响应；本轮没有执行这项实物验收。

返回[图像项目入口](README.md)、[预览操作](02_接线与预览操作.md)。
