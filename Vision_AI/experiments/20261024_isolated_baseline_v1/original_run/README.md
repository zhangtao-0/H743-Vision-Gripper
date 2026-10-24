# 三指夹爪视觉算法最小可运行演示

这是**纯电脑离线演示**，不访问真实摄像头、不控制 H743、不发送运动指令。目的是理解图像→轮廓→中心点→结构化识别结果。

## Windows PowerShell

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-minimal.txt
.\.venv\Scripts\python.exe src\synthetic_demo.py
.\.venv\Scripts\python.exe src\shape_baseline.py
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

如果没有 Python 3.11，可在 `py -0p` 查看已有 Python，再在隔离环境中选兼容解释器；不要覆盖其他工程的 Python。也可选已安装且与依赖兼容的 Python 3.12。

结果在 `outputs/`：`synthetic_scene.png`、`annotated.png`、`detections.json`。识别结果是图像像素坐标，不是机械臂坐标，不得直接用于控制实体电机。

## 后续真实训练

先采集工件照片并在 CVAT 标注成 YOLO 实例分割数据集；确认 `data.yaml` 指向实际图片与标签后，在同一隔离环境按照 PyTorch 官方选择 CPU/GPU 安装，再安装 `ultralytics`，执行：

```powershell
.\.venv\Scripts\python.exe -m pip install ultralytics
.\.venv\Scripts\python.exe training\train_yolo_seg.py --data datasets\workpieces_v1\data.yaml --epochs 5 --imgsz 320 --device cpu
```

命令仅是小规模**训练冒烟测试**，不意味着已收集数据或训练成功。正式训练前先核实标签质量、分组划分与电脑显卡，记录 PyTorch/Ultralytics 版本。`ultralytics` 使用 AGPL-3.0/商业许可体系，商业闭源用途需要独立评估许可。
