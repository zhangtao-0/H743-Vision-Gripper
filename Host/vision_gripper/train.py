"""Train/validate custom parcel segmentation; does not fabricate labels or weights."""
import argparse
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data",type=Path,required=True)
    p.add_argument("--model",default="yolo11n-seg.pt",help="local weights or official pretrained model; first run may download")
    p.add_argument("--device",default="0");p.add_argument("--epochs",type=int,default=100)
    p.add_argument("--batch",type=int,default=8);p.add_argument("--imgsz",type=int,default=320)
    p.add_argument("--project",default=str(Path(__file__).parent/"runs"))
    p.add_argument("--name",default="parcel_seg_v1")
    p.add_argument("--validate",type=Path,help="validate a trained .pt on held-out test sessions instead")
    a=p.parse_args()
    if not a.data.is_file():p.error("Missing dataset.yaml; capture, label and prepare dataset first")
    from ultralytics import YOLO
    if a.validate:
        if not a.validate.is_file():p.error("Missing trained model")
        model=YOLO(str(a.validate),task="segment")
        model.val(data=str(a.data.resolve()),split="test",device=a.device,imgsz=a.imgsz,
                  project=a.project,name=a.name+"_test",exist_ok=False)
    else:
        model=YOLO(a.model,task="segment")
        model.train(data=str(a.data.resolve()),epochs=a.epochs,batch=a.batch,imgsz=a.imgsz,
                    device=a.device,workers=0,seed=42,deterministic=True,patience=20,
                    project=a.project,name=a.name,exist_ok=False,degrees=180,fliplr=.5,mosaic=.5)
if __name__=="__main__":main()
