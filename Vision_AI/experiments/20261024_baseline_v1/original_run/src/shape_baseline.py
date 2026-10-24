"""Illustrative RGB shape localization. Not a production gripper controller."""
import json
import math
from pathlib import Path
import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def detect_shapes(image):
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    foreground = cv2.threshold(gray, 225, 255, cv2.THRESH_BINARY_INV)[1]
    foreground = cv2.morphologyEx(
        foreground, cv2.MORPH_OPEN, np.ones((3, 3), dtype=np.uint8))
    contours, _ = cv2.findContours(foreground, cv2.RETR_EXTERNAL,
                                   cv2.CHAIN_APPROX_SIMPLE)
    items = []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area < 800:
            continue
        perimeter = cv2.arcLength(cnt, True)
        if not perimeter:
            continue
        circularity = 4 * math.pi * area / (perimeter * perimeter)
        hull_area = cv2.contourArea(cv2.convexHull(cnt))
        solidity = area / hull_area if hull_area > 0 else 0
        if circularity > 0.83:
            label = 'cylinder_top_view'
        elif solidity < 0.86:
            label = 'l_bracket_top_view'
        else:
            label = 'block_top_view'
        moments = cv2.moments(cnt)
        if not moments['m00']:
            continue
        cx = moments['m10'] / moments['m00']
        cy = moments['m01'] / moments['m00']
        (rect_cx, rect_cy), (width, height), angle = cv2.minAreaRect(cnt)
        items.append({
            'type': label,
            'center_px': [round(cx, 2), round(cy, 2)],
            'box_center_px': [round(rect_cx, 2), round(rect_cy, 2)],
            'box_size_px': [round(width, 2), round(height, 2)],
            'box_angle_deg': round(angle, 2),
            'area_px2': round(area, 1),
            'circularity': round(circularity, 3),
            'solidity': round(solidity, 3),
        })
    return sorted(items, key=lambda item: item['center_px'][1])


def annotate(image, items):
    canvas = image.copy()
    for i, item in enumerate(items):
        x, y = [int(round(v)) for v in item['center_px']]
        cv2.circle(canvas, (x, y), 5, (0, 0, 0), -1)
        cv2.putText(canvas, f"{i+1} {item['type']}", (x - 50, y - 15),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1, cv2.LINE_AA)
    return canvas


def run(image_path, output_dir):
    image = cv2.imread(str(image_path))
    if image is None:
        raise FileNotFoundError(f'Could not load image: {image_path}')
    output_dir.mkdir(parents=True, exist_ok=True)
    result = detect_shapes(image)
    cv2.imwrite(str(output_dir / 'annotated.png'), annotate(image, result))
    (output_dir / 'detections.json').write_text(
        json.dumps({'coordinate_system': 'image_pixels_only', 'objects': result},
                   ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', default=str(ROOT / 'outputs' / 'synthetic_scene.png'))
    parser.add_argument('--output', default=str(ROOT / 'outputs'))
    args = parser.parse_args()
    objects = run(Path(args.input), Path(args.output))
    print(json.dumps(objects, ensure_ascii=False, indent=2))
    print('WARNING: image pixel coordinates cannot be used as robot motion coordinates')
