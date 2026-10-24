# Archived one-off release script. Current guides include subsequent hardware corrections.
"""Write the reviewed six-key release guides; never touches firmware or hardware."""
from pathlib import Path
H=Path(__file__).resolve().parents[3]
def write(rel,text):
 p=H/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text.strip()+'\n',encoding='utf-8')

keys='''| 逻辑按键 | MCU引脚 | 板上位置 | 正常用途 | 上位机模拟 |
|---|---|---|---|---|
| KEY1 | PA0 | U9-10 | 屏幕上/减值 | `@key 1 tap` |
| KEY2 | PB0 | U9-23 | 屏幕下/加值 | `@key 2 tap` |
| KEY3 | PB12 | U9-47 | 确认/进入 | `@key 3 tap` |
| KEY4 | PB13 | U9-46 | 取消/返回 | `@key 4 tap` |
| KEY5 | PC0 | U9-8 | 原KEY1舵机A0手势 | `@key 5 tap` |
| KEY6 | PC1 | U9-7 | 原KEY2舵机B0手势 | `@key 6 tap` |
| KEY7 / EC11按压 | PC13 | U10-7 | 电机短按换向、长按800ms启停 | `@key 7 tap` |
'''
write('Docs/引脚与接线/01_按键接线与测试.md','''# 七路按键：接线与验证

2026-10-23修订：KEY1～4专用于屏幕，KEY5/6接替舵机手势；EC11按压改称KEY7，PC13接线不变。必须运行本次修订固件才有这套逻辑。

'''+keys+'''
每个普通常开按键均接在对应GPIO与GND之间，使用内部上拉、低电平有效，不需要接VCC。先断电接线；四脚按钮请用万用表找到平时断开、按下导通的两端，同组的两个引脚常常本来就短接。

U9-13/14、U10-1/2/3/4和H1-3是公共地。U9-3/4及H1-4是5V，不能当作地。排针Pin1按原理图确认，GPIO编号不是排针序号。

## 不接实体键先验证

1. 打开USART1上位机或USB CDC文字终端，发送`@input status`并回车。
2. 全部按键松开、机构停止后，发送`@input mode mixed`。physical只接受实体，remote只接受模拟，mixed接受两者。
3. 逐条发送表中KEY1～4的tap命令。单键动作在松开时提交；长按不再反转上下或确认取消的含义。
4. 发送`@keys 12 180`，模拟KEY3+KEY4观测页快捷键；再次发送返回原页面。12是位掩码4+8，不是第12号键。
5. 接好实体键后重复上述操作。按住时查询`@input status`看K1～K6；松开后看到0是正常的。PC13由电机输入服务读取，逻辑KEY7可通过实际行为与模拟验证。

KEY5/6仍经舵机在线、限位及业务状态门控；没有回包时按下不应直接运动。KEY5+6的原校准长按组合为`@keys 48 3500`，涉及运动与机械限位，不要拿它当屏幕测试。电机driver profile0无H桥时拒绝启动，不得将电机M+/M-接GPIO。

EC11旋转A/B为PG2/U10-57、PG3/U10-58，公共端接GND；旋转与PC13按压是三路不同输入。

详见[屏幕操作](../屏幕交互/02_按键页面与快捷操作.md)、[引脚总入口](README.md)和[模拟映射](../输入映射/02_按键与来源映射.md)。本次新增PC0/PC1接线尚待实物验证。
''')
write('Docs/输入映射/01_上位机模拟与按键操作.md','''# 不接按键也能操作同一套菜单

发送方式：ASCII文本，每条命令末尾加回车/换行。先执行`@help keys`，它只向当前上位机打印可复制命令，不自动执行列表。

'''+keys+'''
先执行`@input status`查看模式，再在停止运动、全部按键释放后执行`@input mode mixed`。可用remote纯模拟，也可用physical纯实体；切换有按住或运动门控，拒绝时应先解除占用。

`@key 1 down`与`@key 1 up`用于分开模拟按住/释放，`@key 1 press 1000`模拟1秒；普通操作用tap即可。不同来源各自保持输入状态，某个来源的up不会清掉另一来源的down；超时释放防止串口断开导致一直按住。

`@keys 12 180`是K3+K4快捷进入/返回观测页；`@keys 48 3500`是K5+K6原舵机校准手势，后者涉及机构，不能代替菜单测试。`@keys release`释放当前来源的模拟保持。

打开帮助并复制指令后，再发送所选的一条才执行。USB图像开启时由预览程序独占CDC；需要同时控制请使用USART1上位机，避免两个软件抢同一个COM口。新固件与旧固件的按键编号不同，烧录前后需核对版本。

返回[输入映射入口](README.md)。
''')
write('Docs/输入映射/02_按键与来源映射.md','''# 实体与模拟输入如何汇合

'''+keys+'''
实体GPIO采样和USART1/USB模拟按下先进入Input_Resolve，再由相同去抖、松开事件和业务手势处理。菜单不直接解析串口字符，串口也不绕过限位直接驱动电机。

七位掩码：K1=1、K2=2、K3=4、K4=8、K5=16、K6=32、K7=64，合法范围1～127。K3+4=12；K5+6=48。CSV/JSON是当前分配的机器可读表，不把旧F5子命令编号与实体按键编号混为一谈。

K3+4组合只产生一次快捷事件，组合释放不会再生成确认/取消。快捷进入保存之前页面路径与选中项，按组合再次返回；也可常规返回再进入。屏幕上下与确认取消无须靠长按实现反向操作。

原FF FF FE 07 F5控制帧不改变；A0/B0子命令仍是原业务，只是物理入口改成K5/K6。USB CDC当前只接文本控制台，旧二进制帧继续从USART1/USB-TTL发送，详见[指令手册](../指令手册/README.md)。

[JSON](按键映射.json) / [CSV](按键映射.csv) / [扩展教学](06_扩展开发教程/README.md)。
''')
write('Docs/屏幕交互/02_按键页面与快捷操作.md','''# 四键菜单、观测快捷页与图像查看

当前修订为独立四键：KEY1上、KEY2下、KEY3确认、KEY4取消。单键在释放时提交，长按保持同一角色，不再用长按KEY3/4模拟反向操作。物理位置见[接线表](../引脚与接线/01_按键接线与测试.md)。

主页选择参数调节、指令帮助、常用执行、默认参数、状态观测、图像查看等入口，KEY3进入，KEY4返回。编辑数值时KEY1减、KEY2加、KEY3提交、KEY4放弃。修改运行值不等于保存开机默认；在默认参数页执行保存后才写入持久化配置。

KEY3+KEY4同时按下是观测页快捷方式：从其他页面进入时保存原路径与选中项，再按组合返回原位置。快捷页是普通页面，仍可用KEY4返回及常规导航；组合释放不会附带确认或取消。

图像查看页复用KEY1切换图像左右镜像、KEY2每次转90°、KEY3拍照、KEY4退出。屏幕文字旋转与相机方向分开保存；图像视图变换不会改变电脑采集的原始标签坐标。照片功能的存储条件请按状态检查，不能仅凭按下判定文件已写入。

帮助页按分类将指令打印至上位机，不占满屏幕逐条列出所有参数。执行页用于夹取/松开等常用操作；在线、驱动与安全门控仍然适用。尚未接入的视觉自动抓取不是按一下就能完成的功能。

无实体按键可发送`@key 1 tap`、`@key 2 tap`、`@key 3 tap`、`@key 4 tap`及`@keys 12 180`。KEY5/6是独立舵机入口，KEY7是电机按压。

返回[屏幕入口](README.md)。
''')
write('Docs/指令手册/01_文本控制台/05_舵机诊断与角度等效入口.md','''# 舵机身份、只读诊断与等效角度命令

## 先纠正ID

原servo仓库提交aebb357dd4096350ec17f49fdaf23d1b27decb3d的[feetech_sts_bus.h](https://github.com/zhangtao-labs/servo/blob/aebb357dd4096350ec17f49fdaf23d1b27decb3d/Project/code/feetech_sts_bus.h)定义默认ID=1、广播ID=0xFE（254）；[usart.c](https://github.com/zhangtao-labs/servo/blob/aebb357dd4096350ec17f49fdaf23d1b27decb3d/Project/code/usart.c)配置USART2为1,000,000bps。ID0是普通单播ID，不是广播。本次将H743默认目标从0改回1，并统一到board_config.h；不写舵机EEPROM改变其实际ID。

本地F5帧中FE是H743本地协议地址，不能据此认为舵机ID0是广播。广播一般不能用于等待单个舵机回包。

## 文本入口

| 命令 | 用途及范围 |
|---|---|
| `@servo status` | 查询目标ID、探测ID、在线/业务就绪、波特率、扫描、原始RX与有效包等计数 |
| `@servo probe ID BAUD` | 指定ID0～253和9600～1000000bps进行只读探测；例：`@servo probe 1 1000000` |
| `@servo scan BAUD` | 固定波特率依次单播PING ID0～253；例：`@servo scan 1000000`；无回应约25.4秒后停止 |
| `@servo angle DEG` | 整数0～270度；例：`@servo angle 90`；要求舵机在线和业务就绪，仍受实际限位/所有权门控 |

角度入口调用与原控制相同的业务层，并非直接写PWM。accepted=1表示业务接受请求，不表示物理动作已完成。扫描不是自动更改目标ID；找到其他ID后，应核对接线与身份，再修改集中配置并重新构建，不将未知设备自动纳入运动控制。

已有运动业务就绪时会拒绝重新扫描/探测，以免在线改UART。扫描不写ID/波特率/位置/扭矩寄存器，不自动夹取。`@help claw`向上位机打印本分类示例。

## 本轮实物结果与待核对项

旧固件上ID1/1Mbps只读探测没有增加原始接收字节，有效包仍为0。修改目标ID能纠正软件配置，但当前没有证据证明电气链路已经恢复，更没有舵机运动验证。

用户确认转接板V1输入11.5V、没有TTL/RS485拨码或跳帽；这是转接板输入电压，不能在未知板内电路时等同舵机输出端电压。旧F103曾可用说明应优先对照原接法；仍需确认板型、TTL单线BUS接到哪组端子、舵机口实际电压，以及UART是否同地。MCU PA2 TX接转接板RXD、PA3 RX接TXD。

STS3215-C001已有资料注明6～7.4V；不能因为曾经没坏就把未知的11.5V输出认定为额定安全。请先测量舵机端，再根据实际型号的资料确认。不要带电反插MOTOR/BUS接口，也不要将RS485差分端当TTL端使用。

旧FF FF开头命令继续保留，详见[二进制协议](../README.md)。
''')
write('Docs/视觉识别与抓取/10_零基础从接线到第一张图.md','''# 零基础：从USB连接到电脑第一张实时图

## 你目前完成到哪里

你已烧录上一版USB图像固件，并安装过训练依赖。这不等于训练了模型，也不自动开启USB视频。本轮在板子上确认COM11为H743原生USB（VID0483/PID5740），COM9为CH340。端口号可能随插口变化，不要单凭“USB-SERIAL”这个软件标签判断。

XCAM关闭后，诊断程序执行只读查询并开启2fps视频，在6.19秒内收到7张CRC正确的图像、0个CRC错误。证据见[实际诊断](../整合交付/20261023_六键菜单与舵机图像排查/上一版实物USB诊断.json)及[最后一帧](../整合交付/20261023_六键菜单与舵机图像排查/实物USB最后一帧.png)。这是上一版已烧录固件的结果；新按键固件的编译结果另记，不混为同一次实物验证。

## 1. 连接与释放端口

将开发板原生USB数据口连电脑，保持摄像头连接。关闭XCAM的串口及其他占用COM11的软件。一个COM口通常只能由一个程序独占；VOFA如需同时发控制指令，可放在USART1的CH340口。

这里USB是CDC虚拟串口。程序虽然打开时填写115200，但固件的CDC数据通道没有用它分频出UART比特时钟，改变它不会把USB变成同速UART。USART1的115200、舵机USART2的1Mbps才是实际UART链路参数。

## 2. 启动专用预览

双击[启动预览.cmd](../../Host/vision_gripper/启动预览.cmd)。它使用本工程虚拟环境，查找唯一VID0483/PID5740端口，再打开预览。存在多个同类板时会要求指定端口，不会随意选CH340。

也可打开PowerShell，进入H743目录执行：

```powershell
$vp = '.\\Host\\vision_gripper\\.venv\\Scripts\\python.exe'
& $vp .\\Host\\vision_gripper\\start_viewer.py --port COM11
```

本程序自动发送`@video fps 2`、`@camera live`、`@video on`，因此不需要另开终端手动启动。预览不需要训练模型、不需要GPU。启动后移动手指或纸盒，画面应跟随；首次等待几秒钟。

XCAM当前没有针对本项目H743IMG1帧头、CRC及RGB565载荷的适配，单纯打开串口不会显示本协议图像。请用工程自带程序，不能只靠修改XCAM波特率解决。

## 3. 保存第一张图

点击窗口“Save raw image [S]”，或在窗口获得焦点后按小写s。状态栏显示保存成功才算完成；无新图时会显示No fresh frame。每次启动产生新的session目录，路径为Host/vision_gripper/data/raw/日期时间_随机后缀，每张PNG配有JSON元数据。

保存的是240×240的原始传感器方向RGB图。屏幕旋转、预览翻转不会偷偷改标签坐标；标注用保存的原图。关闭窗口后程序释放COM口并尝试关闭图像流。

## 4. 黑屏时按顺序查

| 现象 | 检查 |
|---|---|
| PermissionError / access denied | XCAM、另一个预览、串口助手是否占用了COM11 |
| 找不到CDC或多块CDC | 先检查数据线与设备管理器；执行start_viewer.py --port 实际端口 |
| 只有文字没有帧 | 是否上一版USB图像固件；文字终端先查询@video status与@status；关闭终端再启动预览 |
| CAM ready=0或frames不增长 | 查摄像头供电、排线方向、FD5640默认固件是否正确 |
| 有RX字节但CRC/帧错误 | 确认本项目viewer/wire与固件协议一致，不使用XCAM拆包 |
| 模型文件不存在 | 先去掉--weights正常预览；完成下一篇训练后再提供best.pt |

没有图像时不要先反复重装torch：串口图像接收、CRC、RGB565解码与模型推理是不同环节。需要留存诊断可执行：

```powershell
& $vp .\\Host\\vision_gripper\\diagnose_usb.py --help
```

诊断程序也要独占串口，不与预览同时运行。下一步看[从图片到训练](11_从图片到训练的逐步教程.md)。
''')
write('Docs/视觉识别与抓取/11_从图片到训练的逐步教程.md','''# 零基础：采集、标注、训练、验证与手持测试

目标先分成纸盒carton与黑色快递袋black_parcel两类，识别可见轮廓后计算二维中心、夹取方向和像素宽度。矩形是几何属性，不必为每一种长宽比单独建类别。电脑负责深度学习，H743负责采集、业务和执行；模型不会因为安装torch就自动认识你的场景。

## 1. 确认环境而不是先训练

以下所有命令在H743目录PowerShell运行。第一行每次打开新窗口重新执行：

```powershell
$vp = '.\\Host\\vision_gripper\\.venv\\Scripts\\python.exe'
& $vp -c "import torch; print(torch.__version__); print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'CPU')"
```

本机查到RTX4060 Laptop、显存约8GB，但最初已装的是torch 2.14.1+cpu，CUDA=False。显卡存在和torch能用GPU是两件事。当前安装/重试的实际结果见[本轮交付记录](../整合交付/20261023_六键菜单与舵机图像排查/README.md)。不要在后台安装尚未结束时同时改依赖；下载校验失败时应重试官方源，不能跳过哈希。

## 2. 采集5个以上独立场次

运行上一章预览，按s保存。一个session就是一次启动产生的目录；每个场次都采集纸盒和黑袋，也采没有快递的背景。至少5个场次，第一轮建议每类累计200～400张有差异的图、另有背景负样本；这是起步建议，不是达到张数就保证准确。

摄像头随手运动，所以要覆盖远近、上下左右偏移、倾斜、转动、不同桌面和光照、手遮挡、黑袋反光与褶皱。每保存一张都先改变视角，不连续保存几乎一样的几十帧。不要把5个相邻帧目录改名伪装成5个场次。

## 3. 用多边形标注

```powershell
& $vp .\\Host\\vision_gripper\\annotate.py
```

窗口底部选择carton或black_parcel，沿当前物体**可见外轮廓**依次单击，至少3点；点击Finish object完成一个多边形。有多个目标就逐个画，完成后点击Save & next保存并到下一图。Backspace撤回当前多边形最后一点，Clear清空当前图的标记重画。

只有完全没有目标的背景才能点Negative & next。手挡住的不可见区域不要凭想象补成完整盒子，别把桌面黑影标为黑袋；所有可见目标都应标完。标签保存在data/labels/相同session/同名.txt。class0=carton，class1=black_parcel，坐标归一化到0～1；空txt代表你明确确认的负样本，缺少txt代表尚未标注。

## 4. 生成数据集并保留测试场次

```powershell
& $vp .\\Host\\vision_gripper\\prepare_dataset.py
```

默认生成data/parcel_v1/dataset.yaml和manifest.json。脚本检查240×240图、多边形、漏标、至少5场次及每个split两类齐全，并按整个session分train/val/test，避免同一视角相邻帧同时进入训练和测试。已有parcel_v1时不覆盖，重做请指定新版本：

```powershell
& $vp .\\Host\\vision_gripper\\prepare_dataset.py --output .\\Host\\vision_gripper\\data\\parcel_v2
```

测试集不能拿来反复挑模型或调阈值；它代表尚未参与训练的场景。出现“lacks carton or black_parcel”时补采对应场次，不能删掉校验强行训练。

## 5. 训练一个小分割模型

确认CUDA=True后：

```powershell
& $vp .\\Host\\vision_gripper\\train.py --data .\\Host\\vision_gripper\\data\\parcel_v1\\dataset.yaml --device 0 --epochs 100 --batch 8 --imgsz 320
```

第一次可能联网下载yolo11n-seg.pt初始权重；它不是你已经训练好的快递模型。显存不足先把batch8改4，仍不足改2。没有GPU可把--device 0改成--device cpu，速度较慢但流程相同。查看输出训练目录，成功产物通常是runs/parcel_seg_v1/weights/best.pt；如果重名自动产生后缀，以本次控制台打印的目录为准。

## 6. 在保留测试集上评价

```powershell
& $vp .\\Host\\vision_gripper\\train.py --data .\\Host\\vision_gripper\\data\\parcel_v1\\dataset.yaml --validate .\\Host\\vision_gripper\\runs\\parcel_seg_v1\\weights\\best.pt --device 0
```

同时看纸盒与黑袋的分割结果，观察漏检、把手当快递、背景误检、轮廓是否贴合。不能只看总mAP；抓取依赖轮廓与中心，框对了而掩码不对仍可能给错方向。补数据后训练新版本，保留每版数据清单与测试结果。

## 7. 手拿摄像头做实时测试

```powershell
& $vp .\\Host\\vision_gripper\\start_viewer.py --port COM11 --weights .\\Host\\vision_gripper\\runs\\parcel_seg_v1\\weights\\best.pt --device 0
```

确认目标覆盖、中心及方向随运动合理变化；停稳时建议应稳定，快速晃动、目标出画面或严重遮挡时应失效。先不加--send-advice，仅在电脑看结果。需要把稳定的二维建议回传H743时再加--send-advice；该功能不发送舵机/电机运动指令。

现在只能验证“画面里哪里可抓、朝哪个方向、像素宽度多少”。移动单目相机尚无标定、深度和机器人坐标变换，像素距离不能直接变成毫米，更不能认为能自动接近物体。手持测试请逐项记录：类别、中心、方向是否合理、停稳有效/移动失效是否及时，下一阶段再做标定、距离和真实夹爪闭环。

所有raw、labels、数据集和runs自行备份；仓库忽略这些大文件不代表已经有远端备份。返回[视觉入口](README.md)。
''')

# Current entry points must not contradict generated data.
for rel in ['Docs/引脚与接线/README.md','引脚接线速查.md']:
 p=H/rel;s=p.read_text(encoding='utf-8-sig').replace('39个','37个')
 if rel.endswith('README.md'):
  s=s.replace('**现在要接菜单键：KEY3接PB12/U9-47，KEY4接PB13/U9-46；每个常开按键另一端接GND。**','**菜单键：KEY1=PA0/U9-10上，KEY2=PB0/U9-23下，KEY3=PB12/U9-47确认，KEY4=PB13/U9-46取消；另一端均接GND。KEY5/6新用PC0/PC1，EC11按压称KEY7。**')
 else:
  start=s.find('| KEY1');end=s.find('\n\n',start)
  if start>=0:s=s[:start]+keys.strip()+s[end:]
 p.write_text(s,encoding='utf-8')
for rel in ['Docs/输入映射/README.md','Docs/视觉识别与抓取/README.md','Host/vision_gripper/README.md']:
 p=H/rel;s=p.read_text(encoding='utf-8-sig')
 if rel=='Docs/输入映射/README.md':
  s=s.replace('版本input-map-v1，2026-10-22。','当前版本20261023-sixkey-v1。').replace('物理引脚和原操作保留。','KEY1～4改为屏幕上下确认取消；新增KEY5/6接替舵机，EC11按压改为KEY7。')
 elif rel=='Docs/视觉识别与抓取/README.md':
  s+='\n## 零基础逐步操作\n\n先看[从USB到第一张图](10_零基础从接线到第一张图.md)，再看[采集到训练及手持测试](11_从图片到训练的逐步教程.md)。\n'
 else:
  s+='\n## 当前电脑快速入口\n\n双击[启动预览.cmd](启动预览.cmd)自动选择唯一H743 CDC并开启视频；也可使用start_viewer.py --port COM11。详细的[零基础步骤](../../Docs/视觉识别与抓取/10_零基础从接线到第一张图.md)解释端口、XCAM与采集。训练环境状态以本轮交付记录为准，未提供虚构的已训练权重。\n'
 p.write_text(s,encoding='utf-8')

if __name__=='__main__':print('Current six-key, servo and beginner vision guides written.')
