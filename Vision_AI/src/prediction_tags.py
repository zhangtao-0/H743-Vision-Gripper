"""Translate model summaries into open evidence tags without inventing physics."""
import math
import cv2
import numpy as np
from tag_reasoner import reason


def interpret(detection, image_size, extra_tags=()):
    width, height = image_size
    if width <= 0 or height <= 0:
        raise ValueError("Positive image dimensions required")
    confidence = float(detection["confidence"])
    if not math.isfinite(confidence) or not 0 <= confidence <= 1:
        raise ValueError("Invalid model confidence")
    tags = [dict(id="semantic:" + detection["name"], confidence=confidence, source="model")]
    geometry = dict(coordinate_system="Pixel", three_dimensional_shape="unknown", material="unknown", mass="unknown")
    segmentation = detection.get("segments", {})
    xs, ys = segmentation.get("x", []), segmentation.get("y", [])
    if len(xs) != len(ys):
        raise ValueError("Malformed polygon")
    if len(xs) >= 3:
        polygon = np.array(list(zip(xs, ys)), dtype=np.float32)
        if not np.isfinite(polygon).all() or polygon.min() < 0 or polygon.max() > 1:
            raise ValueError("Expected normalized model polygon")
        polygon *= (width, height)
        area = cv2.contourArea(polygon)
        if area > 0:
            center, size, angle = cv2.minAreaRect(polygon)
            ratio = max(size) / max(min(size), 1)
            hull_area = cv2.contourArea(cv2.convexHull(polygon))
            geometry.update(center_px=list(center), box_size_px=list(size), angle_deg=angle,
                            area_px2=area, aspect_ratio=ratio, solidity=area / max(hull_area, 1))
            if ratio > 1.7:
                tags.append(dict(id="shape:elongated_2d", confidence=min(confidence, .75), source="inferred"))
            if area / max(hull_area, 1) < .8:
                tags.append(dict(id="shape:concave_2d", confidence=min(confidence, .7), source="inferred"))
    tags.extend(extra_tags)
    return dict(tags=tags, geometry=geometry, decision=reason(tags),
                physics_not_observed=["mass", "friction", "wall_strength", "contents", "temperature"],
                model_confidence_is_not_grasp_success_probability=True)
