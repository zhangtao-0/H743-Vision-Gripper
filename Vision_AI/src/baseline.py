"""Offline teaching baseline. Geometry hints in pixels; never actuator commands.

Adapted from the user's teaching package. The unmodified original is in Resources.
"""
import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
import math
from pathlib import Path

import cv2
import numpy as np


@dataclass(frozen=True)
class Config:
    mode: str = "gray_dark"
    threshold: int = 225
    min_area: float = 800
    hsv_low: tuple = (0, 0, 0)
    hsv_high: tuple = (179, 255, 180)

    def validate(self):
        if self.mode not in ("gray_dark", "gray_light", "hsv"):
            raise ValueError("mode must be gray_dark, gray_light or hsv")
        if not 0 <= self.threshold <= 255 or not math.isfinite(self.min_area) or self.min_area <= 0:
            raise ValueError("Invalid threshold/area")
        if len(self.hsv_low) != 3 or len(self.hsv_high) != 3:
            raise ValueError("HSV needs three components")
        for lo, hi, maximum in zip(self.hsv_low, self.hsv_high, (179, 255, 255)):
            if not 0 <= lo <= hi <= maximum:
                raise ValueError("Invalid OpenCV HSV range")


def read_image(path):
    """imdecode/fromfile also handles Windows Chinese paths."""
    path = Path(path)
    image = cv2.imdecode(np.frombuffer(path.read_bytes(), np.uint8), cv2.IMREAD_COLOR)
    if image is None:
        raise ValueError(f"Not a decodable image: {path}")
    return image


def write_image(path, image):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        raise ValueError(f"Cannot encode {path}")
    path.write_bytes(encoded.tobytes())


def detect(image, config=Config()):
    config.validate()
    if image is None or image.dtype != np.uint8 or image.ndim != 3 or image.shape[2] != 3 or not image.size:
        raise ValueError("Expected nonempty uint8 BGR image")
    if config.mode == "hsv":
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, np.array(config.hsv_low), np.array(config.hsv_high))
    else:
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        kind = cv2.THRESH_BINARY_INV if config.mode == "gray_dark" else cv2.THRESH_BINARY
        mask = cv2.threshold(gray, config.threshold, 255, kind)[1]
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    objects, rejected = [], []
    height, width = image.shape[:2]
    for contour in contours:
        area = cv2.contourArea(contour)
        if area < config.min_area:
            continue
        x, y, w, h = cv2.boundingRect(contour)
        if x == 0 or y == 0 or x + w >= width or y + h >= height:
            rejected.append({"reason": "foreground_touches_image_border", "area_px2": area})
            continue
        perimeter = cv2.arcLength(contour, True)
        moments = cv2.moments(contour)
        if perimeter <= 0 or moments["m00"] <= 0:
            continue
        circularity = 4 * math.pi * area / perimeter ** 2
        hull_area = cv2.contourArea(cv2.convexHull(contour))
        solidity = area / hull_area if hull_area else 0
        shape = "cylinder_top_view" if circularity > .83 else "l_bracket_top_view" if solidity < .86 else "block_top_view"
        center = (moments["m10"] / moments["m00"], moments["m01"] / moments["m00"])
        rect_center, size, angle = cv2.minAreaRect(contour)
        long_side, short_side = max(size), min(size)
        angle = (angle + (90 if size[0] < size[1] else 0) + 90) % 180 - 90
        local_mask = np.zeros_like(mask)
        cv2.drawContours(local_mask, [contour], -1, 255, -1)
        _, _, _, interior = cv2.minMaxLoc(cv2.distanceTransform(local_mask, cv2.DIST_L2, 5))
        objects.append(dict(
            shape_hint=shape, center_px=list(center), center_inside=bool(cv2.pointPolygonTest(contour, center, False) >= 0),
            interior_point_px=list(interior), box_center_px=list(rect_center), box_size_px=list(size),
            box_corners_px=cv2.boxPoints((rect_center, size, cv2.minAreaRect(contour)[2])).tolist(),
            long_axis_angle_deg=angle, orientation_ambiguous=shape == "cylinder_top_view" or long_side / max(short_side, 1) < 1.15,
            area_px2=area, circularity=circularity, solidity=solidity,
            grasp_valid=False, invalid_reason="uncalibrated_no_gripper_geometry",
        ))
    return mask, sorted(objects, key=lambda o: (o["center_px"][1], o["center_px"][0])), rejected


def annotate(image, objects):
    canvas = image.copy()
    for index, obj in enumerate(objects):
        box = np.rint(obj["box_corners_px"]).astype(np.int32)
        cv2.polylines(canvas, [box], True, (0, 170, 0), 2)
        center = tuple(round(x) for x in obj["center_px"])
        inside = tuple(obj["interior_point_px"])
        cv2.drawMarker(canvas, center, (0, 0, 255), cv2.MARKER_CROSS, 14, 2)
        cv2.circle(canvas, inside, 4, (255, 0, 0), -1)
        cv2.putText(canvas, f"{index + 1}: {obj['shape_hint']}", (max(0, center[0] - 70), max(18, center[1] - 16)), cv2.FONT_HERSHEY_SIMPLEX, .45, (0, 0, 0), 1, cv2.LINE_AA)
    return canvas


def run(input_path, output, config=Config(), source_kind="unverified"):
    input_path, output = Path(input_path), Path(output)
    if output.exists():
        raise ValueError("Output exists: choose a new run directory; never overwrite evidence")
    image = read_image(input_path)
    mask, objects, rejected = detect(image, config)
    result = dict(schema="h743.vision.baseline.v1", coordinate_system="Pixel", origin="top_left", x_axis="right", y_axis="down", source_kind=source_kind,
                  image_sha256=hashlib.sha256(input_path.read_bytes()).hexdigest(), input=str(input_path.resolve()),
                  image_size=[image.shape[1], image.shape[0]], config=asdict(config), objects=objects, rejected=rejected,
                  grasp_valid=False, motion_commands_sent=False)
    output.mkdir(parents=True)
    drawn = annotate(image, objects)
    write_image(output / "original.png", image)
    write_image(output / "mask.png", mask)
    write_image(output / "annotated.png", drawn)
    panes = [image.copy(), cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR), drawn.copy()]
    for pane, title in zip(panes, ("INPUT", "MASK", "GEOMETRY - PIXELS ONLY")):
        cv2.rectangle(pane, (0, 0), (pane.shape[1], 27), (255, 255, 255), -1)
        cv2.putText(pane, title, (8, 19), cv2.FONT_HERSHEY_SIMPLEX, .5, (0, 0, 0), 1)
    write_image(output / "comparison.png", np.hstack(panes))
    (output / "detections.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--config", type=Path)
    p.add_argument("--source-kind", choices=("synthetic", "real", "unverified"), default="unverified")
    a = p.parse_args()
    cfg = Config(**json.loads(a.config.read_text(encoding="utf8"))) if a.config else Config()
    result = run(a.input, a.output, cfg, a.source_kind)
    print(json.dumps(dict(output=str(a.output),objects=len(result["objects"]),coordinate_system="Pixel",grasp_valid=False), ensure_ascii=False))


if __name__ == "__main__":
    main()
