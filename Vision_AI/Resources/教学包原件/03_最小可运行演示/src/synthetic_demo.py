"""Create a reproducible tabletop image of three geometries, without cameras or weights."""
from pathlib import Path
import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def create_demo_image():
    image = np.full((480, 640, 3), 245, dtype=np.uint8)
    cv2.circle(image, (148, 138), 52, (55, 90, 150), -1)
    cv2.rectangle(image, (320, 100), (436, 184), (80, 142, 62), -1)
    polygon = np.array([(295, 300), (332, 300), (332, 386),
                        (410, 386), (410, 420), (295, 420)], dtype=np.int32)
    cv2.fillPoly(image, [polygon], (115, 70, 155))
    return image


if __name__ == '__main__':
    out_dir = ROOT / 'outputs'
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / 'synthetic_scene.png'
    cv2.imwrite(str(path), create_demo_image())
    print(f'Wrote {path}')
