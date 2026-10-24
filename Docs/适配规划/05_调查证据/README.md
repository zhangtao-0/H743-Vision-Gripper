# 调查证据说明

日期：2026-10-20。

| 文件 | 用途 |
|---|---|
| [源码差异清单.json](源码差异清单.json) | 33 个 App C/H 文件的本地路径、SHA-256、上游同名候选、按字节比较状态 |
| [上游差异.patch](上游差异.patch) | 可解码的同名源码文本差异；用于审阅，不应直接整体应用 |
| [目标基线哈希.json](目标基线哈希.json) | 调查时目标源码、工程配置与主要参考驱动哈希，便于后续检测变化 |
| [IAR基线构建.log](IAR基线构建.log) | 未修改源码的临时副本的真实 IAR 编译输出（基线 51 errors） |
| [构建摘要.json](构建摘要.json) | 命令、版本、错误/警告数、结果和源码一致性核对 |
| [IAR_P1构建.log](IAR_P1构建.log) | P1 整改后的全量构建日志（0 errors / 1 warning） |
| [P1构建摘要.json](P1构建摘要.json) | P1 构建结果、告警、代码/数据大小、产物路径 |
| [IAR_P2构建.log](IAR_P2构建.log) | P2 LED/按键/HOST 诊断构建日志 |
| [P2构建摘要.json](P2构建摘要.json) | P2 构建结果、HOST 模式、校验依据 |
| [P2主机测试.json](P2主机测试.json) | P2 UART/LED/按键/HOST 主机边界测试 |
| [IAR_P3构建.log](IAR_P3构建.log) | P3a 只读舵机 + VOFA 遥测构建日志 |
| [P3构建摘要.json](P3构建摘要.json) | P3a 构建结果、只读模式、代码/数据大小 |
| [P3主机测试.json](P3主机测试.json) | P3a 协议/状态机/无写操作主机测试 |
| [基线构建位置.txt](基线构建位置.txt) | 临时工程副本位置，仅为本机调查缓存，不是长期构建入口 |
| [原理图_第1页.png](原理图_第1页.png) | 芯路城鹿小班 `LXB743ZI-P1原理图.pdf` 第 1 页整页渲染 |
| [原理图_IO引出.png](原理图_IO引出.png) | U9/U10 2×30 排针的引脚与网络名 |
| [原理图_显示与摄像头.png](原理图_显示与摄像头.png) | U7 SPI 液晶 FPC10 与 FPC1 DCMI 的引脚定义 |
| [原理图_H1_SWD与USART1.png](原理图_H1_SWD与USART1.png) | H1 2×4 排针：1 SWCLK、2 SWDIO、3 GND、4 +5V、5 RX、6 TX、7 RST、8 3.3V |
| [原理图_第1页_提取.txt](原理图_第1页_提取.txt) | PDF 文本层提取，用于交叉核对网络名 |

上游：`https://github.com/zhangtao-labs/servo.git`，commit `aebb357dd4096350ec17f49fdaf23d1b27decb3d`。使用 Git 实际克隆并核对，不依赖网页摘要。没有覆盖本地 App。

构建方式：将 `project` 复制到临时路径，不复制既有 `Obj/List/BrowseInfo`；使用 EWP 实际配置名 `H743_VisionGripper` 构建。首次试探使用 `Debug` 配置名时工具报告该配置不存在，随后读取 EWP 更正并执行真实构建；当前日志是更正后的结果。

```powershell
& 'C:\IAR Systems\Embedded Workbench 9.2\common\bin\iarbuild.exe' '<临时副本>\project\EWARM\H743_VisionGripper.ewp' -build H743_VisionGripper -log all
```

基线结果：失败，51 errors / 15 warnings。P1 整改后（2026-10-20）同一配置为 **0 errors / 1 warning，Build succeeded**，见 [IAR_P1构建.log](IAR_P1构建.log) 与 [P1构建摘要.json](P1构建摘要.json)。这只是编译/链接证据，不表示板上外设或机械动作已验证。

差异清单采用同名候选定位，并不是完整仓库双向镜像比较；上游独有库、主程序和启动代码需通过工程依赖继续审查。哈希证明文件内容身份，不证明功能正确。临时路径可能被系统清理，长期保留的是本目录日志与清单。

## P2 接续证据

- [P2修改前哈希.json](P2修改前哈希.json)、[P2修改明细.json](P2修改明细.json)：保留 P1，记录本轮修改前后文件身份。
- [IAR_P2构建.log](IAR_P2构建.log)、[P2构建摘要.json](P2构建摘要.json)：当前版本构建结果。
- [P2主机测试.json](P2主机测试.json)：可执行 HAL stub 测试；不等于硬件测试。
- [上游UART配置证据.txt](上游UART配置证据.txt)：原仓库 USART1/2/3 角色与波特率，commit 固定。
