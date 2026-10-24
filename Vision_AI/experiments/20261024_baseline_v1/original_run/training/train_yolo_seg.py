"""Optional pilot training; only runs if user explicitly invokes it with real labeled data."""
import argparse
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Pilot Ultralytics segmentation training')
    parser.add_argument('--data', required=True, help='Actual labeled dataset YAML')
    parser.add_argument('--model', default='yolo26n-seg.pt')
    parser.add_argument('--epochs', type=int, default=5)
    parser.add_argument('--imgsz', type=int, default=320)
    parser.add_argument('--device', default='cpu')
    parser.add_argument('--batch', type=int, default=4)
    args = parser.parse_args()
    if not Path(args.data).is_file():
        parser.error('Dataset YAML does not exist; do not run training without labels')
    from ultralytics import YOLO
    model = YOLO(args.model)
    model.train(data=args.data, epochs=args.epochs, imgsz=args.imgsz,
                device=args.device, batch=args.batch, seed=42,
                project='experiments', name='pilot_seg', exist_ok=False)

if __name__ == '__main__':
    main()
