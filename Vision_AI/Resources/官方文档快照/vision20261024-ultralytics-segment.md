[Ultralytics YOLO27:Join the waitlist.Coming soon. Join the waitlist.](https://www.ultralytics.com/yolo/yolo27)

 [Skip to main content](https://docs.ultralytics.com/tasks/segment#main-content)

- [Ultralytics](https://docs.ultralytics.com/)

Search...Ctrl K

- [Home](https://docs.ultralytics.com/)
- [Quickstart](https://docs.ultralytics.com/quickstart)

- Home
- Modes
- Tasks


  - [Overview](https://docs.ultralytics.com/tasks)
  - [Detect](https://docs.ultralytics.com/tasks/detect)
  - [Segment](https://docs.ultralytics.com/tasks/segment)
  - [Semantic](https://docs.ultralytics.com/tasks/semantic)
  - [Depth](https://docs.ultralytics.com/tasks/depth)
  - [Classify](https://docs.ultralytics.com/tasks/classify)
  - [Pose](https://docs.ultralytics.com/tasks/pose)
  - [OBB](https://docs.ultralytics.com/tasks/obb)

- Models
- Compare
- Datasets
- Solutions
- Guides
- Integrations
- Platform
- [Rust Inference](https://docs.ultralytics.com/inference)
- Reference
- Help

### Get Started

Annotate, train and deploy YOLO models in one click.

- Cloud training
- Dataset annotation
- One-click inference

[$25 free creditswith work email](https://platform.ultralytics.com/signup)

[Get Started](https://platform.ultralytics.com/signup) [Sign in](https://docs.ultralytics.com/signin?redirect_url=%2Ftasks%2Fsegment)

- LoadingPlease wait


[Edit page on GitHub](https://github.com/ultralytics/ultralytics/blob/main/docs/en/tasks/segment.md)

# Instance Segmentation with Ultralytics YOLO [\#](https://docs.ultralytics.com/tasks/segment\#instance-segmentation)

![Ultralytics YOLO instance segmentation examples](https://cdn.ul.run/i/cf52e30c0986666e4e850b17e0514a54.avif)

[Instance segmentation](https://www.ultralytics.com/glossary/instance-segmentation) goes a step further than object detection and involves identifying individual objects in an image and segmenting them from the rest of the image.

The output of an instance segmentation model is a set of masks or contours that outline each object in the image, along with class labels and confidence scores for each object. Instance segmentation is useful when you need to know not only where objects are in an image, but also what their exact shape is.

![](https://i.ytimg.com/vi_webp/o4Zd-IeMlSY/maxresdefault.webp)

**Watch:** Run Segmentation with Pretrained Ultralytics YOLO Model in Python.

Tip

YOLO26 Segment models use the `-seg` suffix, i.e., `yolo26n-seg.pt`, and are pretrained on [COCO](https://github.com/ultralytics/ultralytics/blob/main/ultralytics/cfg/datasets/coco.yaml).

## [Models](https://github.com/ultralytics/ultralytics/tree/main/ultralytics/cfg/models/26) [\#](https://docs.ultralytics.com/tasks/segment\#models)

YOLO26 Segment models pretrained on the [COCO](https://github.com/ultralytics/ultralytics/blob/main/ultralytics/cfg/datasets/coco.yaml) dataset are shown below.

[Models](https://github.com/ultralytics/ultralytics/tree/main/ultralytics/cfg/models) download automatically from the latest Ultralytics [release](https://github.com/ultralytics/assets/releases) on first use.

| Model | size<br>(pixels) | mAPbox<br>50-95(e2e) | mAPmask<br>50-95(e2e) | Speed<br>CPU ONNX<br>(ms) | Speed<br>T4 TensorRT10<br>(ms) | params<br>(M) | FLOPs<br>(B) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [YOLO26n-seg](https://platform.ultralytics.com/ultralytics/yolo26/yolo26n-seg) | 640 | 39.6 | 33.9 | **53.3 ± 0.5** | **2.1 ± 0.0** | **2.7** | **9.3** |
| [YOLO26s-seg](https://platform.ultralytics.com/ultralytics/yolo26/yolo26s-seg) | 640 | 47.3 | 40.0 | 118.4 ± 0.9 | 3.3 ± 0.0 | 10.4 | 34.5 |
| [YOLO26m-seg](https://platform.ultralytics.com/ultralytics/yolo26/yolo26m-seg) | 640 | 52.5 | 44.1 | 328.2 ± 2.4 | 6.7 ± 0.1 | 23.6 | 121.7 |
| [YOLO26l-seg](https://platform.ultralytics.com/ultralytics/yolo26/yolo26l-seg) | 640 | 54.4 | 45.5 | 387.0 ± 3.7 | 8.0 ± 0.1 | 28.0 | 140.1 |
| [YOLO26x-seg](https://platform.ultralytics.com/ultralytics/yolo26/yolo26x-seg) | 640 | **56.5** | **47.0** | 787.0 ± 6.8 | 16.4 ± 0.1 | 62.8 | 314.0 |

- **mAPval** values are for single-model single-scale on [COCO val2017](https://cocodataset.org/) dataset.

Reproduce with `yolo segment val data=coco.yaml device=0 nms=False`
- **Speed** averaged over COCO val images with ONNX on CPU and TensorRT10 on an NVIDIA T4 GPU.

Reproduce with `yolo segment val data=coco.yaml batch=1 device=0|cpu nms=False`
- **Params** and **FLOPs** values are for fused models after Conv/BatchNorm folding and removal of the unused detection branch. Pretrained checkpoints retain the full training architecture and may show higher counts.

These checkpoints segment the 80 COCO classes. To segment categories outside that list without retraining, see [YOLOE](https://docs.ultralytics.com/models/yoloe), which takes the classes as a text prompt, a visual example, or a built-in vocabulary.

See the [unreleased YOLO27 preview](https://docs.ultralytics.com/models/yolo27#performance-metrics) for preliminary COCO box and mask results.

## Train [\#](https://docs.ultralytics.com/tasks/segment\#train)

Train YOLO26n-seg on the COCO8-seg dataset for 100 [epochs](https://www.ultralytics.com/glossary/epoch) at image size 640. For a full list of available arguments see the [Configuration](https://docs.ultralytics.com/usage/cfg) page.

Example

PythonCLI

```
from ultralytics import YOLO

# Load a model
model = YOLO("yolo26n-seg.yaml")  # build a new model from YAML
model = YOLO("yolo26n-seg.pt")  # load a pretrained model (recommended for training)
model = YOLO("yolo26n-seg.yaml").load("yolo26n-seg.pt")  # build from YAML and transfer weights

# Train the model
results = model.train(data="coco8-seg.yaml", epochs=100, imgsz=640)
```

See full `train` mode details in the [Train](https://docs.ultralytics.com/modes/train) page. Segmentation models can also be trained with [Ultralytics Platform cloud training](https://docs.ultralytics.com/platform/train/cloud-training).

### Dataset format [\#](https://docs.ultralytics.com/tasks/segment\#dataset-format)

YOLO segmentation dataset format can be found in detail in the [Dataset Guide](https://docs.ultralytics.com/datasets/segment). To convert an existing COCO JSON dataset to YOLO format, use the built-in `convert_coco` function with `use_segments=True`, as described in the [COCO to YOLO guide](https://docs.ultralytics.com/guides/coco-to-yolo). You can also create segmentation masks with [Ultralytics Platform annotation](https://docs.ultralytics.com/platform/data/annotation) using polygon tools and SAM-powered smart annotation.

## Val [\#](https://docs.ultralytics.com/tasks/segment\#val)

Validate trained YOLO26n-seg model [accuracy](https://www.ultralytics.com/glossary/accuracy). No arguments are needed, as the `model` retains its training `data` and arguments as model attributes: `path/to/best.pt` from the [Train](https://docs.ultralytics.com/tasks/segment#train) example validates on COCO8-seg. Official weights record a training dataset path that doesn't exist on your machine, so they fall back to the task default `coco8-seg.yaml` with a warning. Pass `data` to validate on another dataset.

Example

PythonCLI

```
from ultralytics import YOLO

# Load a model
model = YOLO("yolo26n-seg.pt")  # load an official model
model = YOLO("path/to/best.pt")  # load a custom model

# Validate the model
metrics = model.val()  # no arguments needed, dataset and settings remembered
metrics.box.map  # map50-95(B)
metrics.box.map50  # map50(B)
metrics.box.map75  # map75(B)
metrics.box.maps  # a list containing mAP50-95(B) for each category
metrics.box.image_metrics  # per-image metrics dictionary for det with precision, recall, F1, TP, FP, and FN
metrics.seg.map  # map50-95(M)
metrics.seg.map50  # map50(M)
metrics.seg.map75  # map75(M)
metrics.seg.maps  # a list containing mAP50-95(M) for each category
metrics.seg.image_metrics  # per-image metrics dictionary for seg with precision, recall, F1, TP, FP, and FN
```

## Predict [\#](https://docs.ultralytics.com/tasks/segment\#predict)

Use a trained YOLO26n-seg model to run predictions on images.

Example

PythonCLI

```
from ultralytics import YOLO

# Load a model
model = YOLO("yolo26n-seg.pt")  # load an official model
model = YOLO("path/to/best.pt")  # load a custom model

# Predict with the model
results = model("https://ultralytics.com/images/bus.jpg")  # predict on an image

# Access the results
for result in results:
    xy = result.masks.xy  # mask polygons in pixel coordinates
    xyn = result.masks.xyn  # normalized mask polygons
    masks = result.masks.data  # binary masks, shape (N,H,W), dtype torch.uint8
```

See full `predict` mode details in the [Predict](https://docs.ultralytics.com/modes/predict) page.

### Results Output [\#](https://docs.ultralytics.com/tasks/segment\#results-output)

YOLO instance segmentation returns one `Results` object per image. Each result stores object-level predictions, where
each detected instance has its own binary mask, class, confidence, and box.

| Attribute | Type | Shape | Description |
| --- | --- | --- | --- |
| `result.masks` | `Masks` | `(N)` | Instance masks. |
| `result.masks.data` | `torch.uint8` | `(N,H,W)` | Binary masks, values `0` or `1`. |
| `result.masks.xy` | `np.float32` | `list[(P,2)]` | Pixel polygons. |
| `result.masks.xyn` | `np.float32` | `list[(P,2)]` | Normalized polygons. |
| `result.boxes` | `Boxes` | `(N)` | Instance boxes/classes/confidences. |
| `result.boxes.cls` | `torch.float32` | `(N,)` | Class IDs; cast to `int` for names. |

For task-specific `Results` fields across every task, see the [Predict Results by Task](https://docs.ultralytics.com/modes/predict#results-by-task) section.

### Instance vs Semantic Segmentation [\#](https://docs.ultralytics.com/tasks/segment\#instance-vs-semantic-segmentation)

Instance segmentation is object-level segmentation: two cars produce two masks, two boxes, and two confidence scores.
[Semantic segmentation](https://docs.ultralytics.com/tasks/semantic) is pixel-level classification: those same cars become pixels with the same class ID
in one image-sized class map, with no per-object boxes, confidences, or default polygon list. See the
[field-by-field comparison](https://docs.ultralytics.com/tasks/semantic#instance-vs-semantic-segmentation) on the semantic segmentation page.

## Export [\#](https://docs.ultralytics.com/tasks/segment\#export)

Export a YOLO26n-seg model to a different format like ONNX, CoreML, etc.

Example

PythonCLI

```
from ultralytics import YOLO

# Load a model
model = YOLO("yolo26n-seg.pt")  # load an official model
model = YOLO("path/to/best.pt")  # load a custom model

# Export the model
model.export(format="onnx")
```

Available YOLO26-seg export formats are in the table below. You can export to any format using the `format` argument, i.e., `format='onnx'` or `format='engine'`. You can predict or validate directly on exported models, i.e., `yolo predict model=yolo26n-seg.onnx`. Usage examples are shown for your model after export completes.

Note

CoreML supports embedded NMS (`nms=True`) for detection, instance segmentation and pose with static shapes. Default segmentation exports (`nms=None`) leave NMS to the consumer.

| Format | `format` Argument | Model | Metadata | Arguments |
| --- | --- | --- | --- | --- |
| [PyTorch](https://pytorch.org/) | - | `yolo26n-seg.pt` | ✅ | - |
| [TorchScript](https://docs.ultralytics.com/integrations/torchscript) | `torchscript` | `yolo26n-seg.torchscript` | ✅ | `imgsz`, `quantize`, `dynamic`, `nms`, `batch`, `device` |
| [ONNX](https://docs.ultralytics.com/integrations/onnx) | `onnx` | `yolo26n-seg.onnx` | ✅ | `imgsz`, `quantize`, `dynamic`, `simplify`, `opset`, `nms`, `batch`, `data`, `fraction`, `device` |
| [OpenVINO](https://docs.ultralytics.com/integrations/openvino) | `openvino` | `yolo26n-seg_openvino_model/` | ✅ | `imgsz`, `quantize`, `dynamic`, `nms`, `batch`, `data`, `fraction`, `device` |
| [TensorRT](https://docs.ultralytics.com/integrations/tensorrt) | `engine` | `yolo26n-seg.engine` | ✅ | `imgsz`, `quantize`, `dynamic`, `simplify`, `opset`, `workspace`, `nms`, `batch`, `data`, `fraction`, `device` |
| [CoreML](https://docs.ultralytics.com/integrations/coreml) | `coreml` | `yolo26n-seg.mlpackage` | ✅ | `imgsz`, `dynamic`, `quantize`, `nms`, `batch`, `device` |
| [Apple Core AI](https://docs.ultralytics.com/integrations/coreai) | `coreai` | `yolo26n-seg.aimodel` | ✅ | `imgsz`, `batch`, `quantize` |
| [TF SavedModel](https://docs.ultralytics.com/integrations/tf-savedmodel) | `saved_model` | `yolo26n-seg_saved_model/` | ✅ | `imgsz`, `quantize`, `opset`, `nms`, `batch`, `data`, `fraction`, `device` |
| [TF GraphDef](https://docs.ultralytics.com/integrations/tf-graphdef) | `pb` | `yolo26n-seg.pb` | ❌ | `imgsz`, `opset`, `batch`, `device` |
| [TF Edge TPU](https://docs.ultralytics.com/integrations/edge-tpu) | `edgetpu` | `yolo26n-seg_edgetpu.tflite` | ✅ | `imgsz`, `quantize`, `opset`, `data`, `fraction`, `device` |
| [LiteRT](https://docs.ultralytics.com/integrations/litert) | `litert` | `yolo26n-seg.tflite` | ✅ | `imgsz`, `quantize`, `batch`, `data`, `fraction`, `device` |
| [PaddlePaddle](https://docs.ultralytics.com/integrations/paddlepaddle) | `paddle` | `yolo26n-seg_paddle_model/` | ✅ | `imgsz`, `batch`, `device` |
| [MNN](https://docs.ultralytics.com/integrations/mnn) | `mnn` | `yolo26n-seg.mnn` | ✅ | `imgsz`, `batch`, `dynamic`, `quantize`, `simplify`, `opset`, `nms`, `device` |
| [NCNN](https://docs.ultralytics.com/integrations/ncnn) | `ncnn` | `yolo26n-seg_ncnn_model/` | ✅ | `imgsz`, `quantize`, `batch`, `device` |
| [IMX500](https://docs.ultralytics.com/integrations/sony-imx500) | `imx` | `yolo11n_imx_model/` | ✅ | `imgsz`, `quantize`, `data`, `fraction`, `nms`, `device` |
| [RKNN](https://docs.ultralytics.com/integrations/rockchip-rknn) | `rknn` | `yolo26n-seg_rknn_model/` | ✅ | `imgsz`, `batch`, `name`, `quantize`, `simplify`, `opset`, `data`, `fraction`, `device` |
| [ExecuTorch](https://docs.ultralytics.com/integrations/executorch) | `executorch` | `yolo26n-seg_executorch_model/` | ✅ | `imgsz`, `batch`, `device` |
| [Axelera](https://docs.ultralytics.com/integrations/axelera) | `axelera` | `yolo26n-seg_axelera_model/` | ✅ | `imgsz`, `batch`, `quantize`, `data`, `fraction`, `device` |
| [DEEPX](https://docs.ultralytics.com/integrations/deepx) | `deepx` | `yolo26n-seg_deepx_model/` | ✅ | `imgsz`, `quantize`, `simplify`, `opset`, `data`, `optimize`, `device` |
| [Qualcomm QNN](https://docs.ultralytics.com/integrations/qnn) | `qnn` | `yolo26n-seg_qnn.onnx` | ✅ | `imgsz`, `batch`, `name`, `quantize`, `simplify`, `opset`, `data`, `fraction`, `device` |
| [Hailo](https://docs.ultralytics.com/integrations/hailo) | `hailo` | `yolo26n-seg_hailo_model/` | ✅ | `imgsz`, `name`, `quantize`, `data`, `fraction`, `simplify`, `conf`, `iou`, `device` |
| [Huawei Ascend](https://docs.ultralytics.com/integrations/ascend) | `ascend` | `yolo26n-seg_ascend_model/` | ✅ | `imgsz`, `batch`, `name`, `quantize`, `opset`, `simplify`, `nms`, `device` |
| [AMD Xilinx](https://docs.ultralytics.com/integrations/xilinx) | `xilinx` | `yolo26n-seg_xilinx_model/` | ✅ | `imgsz`, `name`, `quantize`, `data`, `fraction`, `opset`, `simplify`, `device` |

`nms=None` defaults to raw outputs for external NMS (YOLO26 keeps its NMS-free head on [Axelera](https://docs.ultralytics.com/integrations/axelera#supported-tasks)). Set `nms=False` to select an available NMS-free head; unsupported formats fall back to their native output path. The `nms` entries above identify formats that can embed NMS with `nms=True`.

See full `export` details in the [Export](https://docs.ultralytics.com/modes/export) page.

## FAQ [\#](https://docs.ultralytics.com/tasks/segment\#faq)

- ### How do I train a YOLO26 segmentation model on a custom dataset?








To train a YOLO26 segmentation model on a custom dataset, you first need to prepare your dataset in the YOLO segmentation format. You can use the built-in [`convert_coco`](https://docs.ultralytics.com/guides/coco-to-yolo) utility to convert COCO JSON datasets. Once your dataset is ready, you can train the model using Python or CLI commands:



Example







PythonCLI







```
from ultralytics import YOLO

# Load a pretrained YOLO26 segment model
model = YOLO("yolo26n-seg.pt")

# Train the model
results = model.train(data="path/to/your_dataset.yaml", epochs=100, imgsz=640)
```











Check the [Configuration](https://docs.ultralytics.com/usage/cfg) page for more available arguments.

- ### What is the difference between object detection and instance segmentation in YOLO26?








Object detection identifies and localizes objects within an image by drawing bounding boxes around them, whereas instance segmentation not only identifies the bounding boxes but also delineates the exact shape of each object. YOLO26 instance segmentation models provide masks or contours that outline each detected object, which is particularly useful for tasks where knowing the precise shape of objects is important, such as medical imaging or autonomous driving.

- ### Why use YOLO26 for instance segmentation?








Ultralytics YOLO26 is a state-of-the-art model recognized for its high accuracy and real-time performance, making it ideal for instance segmentation tasks. YOLO26 Segment models come pretrained on the [COCO dataset](https://github.com/ultralytics/ultralytics/blob/main/ultralytics/cfg/datasets/coco.yaml), ensuring robust performance across a variety of objects. Additionally, YOLO supports training, validation, prediction, and export functionalities with seamless integration, making it highly versatile for both research and industry applications.

- ### How do I load and validate a pretrained YOLO segmentation model?








Loading and validating a pretrained YOLO segmentation model is straightforward. Here's how you can do it using both Python and CLI:



Example







PythonCLI







```
from ultralytics import YOLO

# Load a pretrained model
model = YOLO("yolo26n-seg.pt")

# Validate the model
metrics = model.val()
print("Mean Average Precision for boxes:", metrics.box.map)
print("Mean Average Precision for masks:", metrics.seg.map)
```











These steps will provide you with validation metrics like [Mean Average Precision](https://www.ultralytics.com/glossary/mean-average-precision-map) (mAP), crucial for assessing model performance.

- ### How can I export a YOLO segmentation model to ONNX format?








Exporting a YOLO segmentation model to ONNX format is simple and can be done using Python or CLI commands:



Example







PythonCLI







```
from ultralytics import YOLO

# Load a pretrained model
model = YOLO("yolo26n-seg.pt")

# Export the model to ONNX format
model.export(format="onnx")
```











For more details on exporting to various formats, refer to the [Export](https://docs.ultralytics.com/modes/export) page.


Contributors

[Edit page on GitHub](https://github.com/ultralytics/ultralytics/blob/main/docs/en/tasks/segment.md)

[![glenn-jocher](https://avatars.githubusercontent.com/u/26833433?s=48)glenn-jocher37](https://github.com/glenn-jocher) [![raimbekovm](https://avatars.githubusercontent.com/u/144048515?s=48)raimbekovm6](https://github.com/raimbekovm) [![Burhan-Q](https://avatars.githubusercontent.com/u/62214284?s=48)Burhan-Q4](https://github.com/Burhan-Q) [![francesco-mttl](https://avatars.githubusercontent.com/u/3855193?s=48)francesco-mttl3](https://github.com/francesco-mttl) [![Y-T-G](https://avatars.githubusercontent.com/u/32206511?s=48)Y-T-G2](https://github.com/Y-T-G) [![miles-deans-ultralytics](https://avatars.githubusercontent.com/u/231651492?s=48)miles-deans-ultralytics1](https://github.com/miles-deans-ultralytics) [![Laughing-q](https://avatars.githubusercontent.com/u/61612323?s=48)Laughing-q1](https://github.com/Laughing-q) [![amanharshx](https://avatars.githubusercontent.com/u/97787149?s=48)amanharshx1](https://github.com/amanharshx) [![pderrenger](https://avatars.githubusercontent.com/u/107626595?s=48)pderrenger1](https://github.com/pderrenger) [![jk4e](https://avatars.githubusercontent.com/u/116908874?s=48)jk4e1](https://github.com/jk4e) [![MatthewNoyce](https://avatars.githubusercontent.com/u/131261051?s=48)MatthewNoyce1](https://github.com/MatthewNoyce) [![RizwanMunawar](https://avatars.githubusercontent.com/u/62513924?s=48)RizwanMunawar1](https://github.com/RizwanMunawar)

CreatedNov 12, 2023Updated1 week ago

## Comments

Ask AIUltralytics