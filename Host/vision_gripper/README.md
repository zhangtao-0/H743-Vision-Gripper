# 电脑端快递视觉工具

主教学入口：[H743图像与抓取](../../Docs/视觉识别与抓取/README.md)。本程序负责USB预览/采集、分割推理与二维抓取建议；不发送运动指令。

| 文件 | 职责 |
|---|---|
| viewer.py / 启动预览.ps1 | 独占USB CDC，预览与S采集，可选--weights和--send-advice |
| wire.py | 帧头/CRC/拆包，RGB565解码及视图变换 |
| grasp.py | 自定义实例分割、掩码几何与连续稳定检查 |
| annotate.py | 本地多边形标注，carton/black_parcel |
| prepare_dataset.py | 标签检查、按session划分train/val/test、哈希清单 |
| train.py | 训练与保留test集验证，默认YOLO11n-seg |
| evaluate_grasps.py | 人工二维参考一致性评估 |
| test_vision.py / test_viewer_gui.py | 主机算法/数据测试与假串口隐藏GUI测试 |

本机已建立.venv预览环境（Python3.12）；训练依赖当前为CPU版，CUDA安装未通过下载校验，自定义权重尚未训练。换电脑可用Python3.12创建本目录.venv，再安装requirements-viewer.txt；训练依赖按[实操手册](../../Docs/视觉识别与抓取/04_深度学习实操.md)另行准备。

```powershell
# 在本目录执行；COM12需改成真实USB CDC端口
.\.venv\Scripts\python.exe viewer.py --list-ports
.\.venv\Scripts\python.exe viewer.py --port COM12
.\.venv\Scripts\python.exe test_vision.py
.\.venv\Scripts\python.exe test_viewer_gui.py
```

数据路径以脚本目录为基准，原图保留传感器方向；屏幕旋转不改变标签坐标。data和runs已加入本目录.gitignore，不会自动上传；请自行备份。没有自定义模型时只预览/采集，不会调用一个伪识别结果。

## 当前电脑快速入口

双击[启动预览.cmd](启动预览.cmd)自动选择唯一H743 CDC并开启视频；也可使用start_viewer.py --port COM11。详细的[零基础步骤](../../Docs/视觉识别与抓取/10_零基础从接线到第一张图.md)解释端口、XCAM与采集。训练环境状态以本轮交付记录为准，未提供虚构的已训练权重。
