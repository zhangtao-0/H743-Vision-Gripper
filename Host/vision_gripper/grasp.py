"""Mask-based planar grasp proposals, never a 3-D pose or actuator command."""
from dataclasses import dataclass
import math
import time
import numpy as np

@dataclass
class Proposal:
    label: str
    score: float
    u: float
    v: float
    angle: float  # jaw closing axis, image x right/y down, [-90,90)
    width: float  # pixels, not mm
    endpoints: tuple
    stable: bool = False
    reason: str = "hold camera still"

def from_mask(mask, label, score, obstacles=None):
    import cv2
    if not math.isfinite(float(score)) or not 0 <= float(score) <= 1:
        return None
    mask = (np.asarray(mask) > 0).astype(np.uint8)
    if mask.ndim != 2 or mask.sum() < 150:
        return None
    count, components, stats, _ = cv2.connectedComponentsWithStats(mask, 8)
    if count <= 1:
        return None
    mask = (components == (1 + np.argmax(stats[1:, cv2.CC_STAT_AREA]))).astype(np.uint8)
    ys, xs = np.nonzero(mask)
    if min(xs.min(), ys.min()) < 3 or xs.max() >= mask.shape[1]-3 or ys.max() >= mask.shape[0]-3:
        return None  # cropped object: no visible complete grasp geometry
    xy = np.column_stack((xs, ys)).astype(float)
    eig, axes = np.linalg.eigh(np.cov(xy, rowvar=False))
    if eig[0] <= 0 or eig[1] / eig[0] < 1.2:
        return None  # near-square/round orientation is ambiguous; reject rather than jitter
    short = axes[:, 0]
    angle = (math.degrees(math.atan2(short[1], short[0])) + 90) % 180 - 90
    unit = np.array([math.cos(math.radians(angle)), math.sin(math.radians(angle))])
    distance = cv2.distanceTransform(mask, cv2.DIST_L2, 5)
    safe_y, safe_x = np.nonzero(distance >= max(3.0, float(distance.max()) * .8))
    if not len(safe_x):
        return None
    center0 = xy.mean(axis=0)
    safe = np.column_stack((safe_x, safe_y))
    center = safe[np.argmin(((safe-center0)**2).sum(axis=1))].astype(float)
    lengths = []
    for sign in (-1, 1):
        length = 0.0
        for step in np.arange(.5, max(mask.shape)*1.5, .5):
            p = center + sign * unit * step
            x, y = np.rint(p).astype(int)
            if x < 0 or y < 0 or y >= mask.shape[0] or x >= mask.shape[1] or not mask[y, x]:
                length = float(step)
                break
        if length <= 0:
            return None
        lengths.append(length + 3.0)
    ends = tuple(tuple(center + sign * unit * length) for sign, length in zip((-1, 1), lengths))
    if any(not (1 <= x < mask.shape[1]-1 and 1 <= y < mask.shape[0]-1) for x, y in ends):
        return None
    if obstacles is not None:
        for t in np.linspace(-lengths[0], lengths[1], int(sum(lengths))*2+1):
            x, y = np.rint(center + unit*t).astype(int)
            if obstacles[y, x]:
                return None
    return Proposal(label, float(score), *center, angle, sum(lengths), ends)

class Stability:
    def __init__(self, samples=4):
        self.samples = samples
        self.history = []
        self.last_time = None

    def reset(self):
        self.history.clear()
        self.last_time = None

    def update(self, p, now=None):
        now = time.monotonic() if now is None else now
        if p is None:
            self.reset()
            return None
        if self.last_time is not None and now-self.last_time > 1.5:
            self.history.clear()
        self.last_time = now
        if self.history:
            q = self.history[-1]
            da = abs((p.angle-q.angle+90) % 180 - 90)
            if p.label != q.label or math.hypot(p.u-q.u,p.v-q.v)>7 or da>10 or abs(p.width-q.width)>10:
                self.history.clear()
        self.history.append(p)
        self.history = self.history[-self.samples:]
        # Compare whole window too: slow drift must not pass adjacent-only checks.
        stable = len(self.history) == self.samples
        for q in self.history:
            stable &= (math.hypot(p.u-q.u,p.v-q.v)<=7 and abs((p.angle-q.angle+90)%180-90)<=10)
        p.stable = bool(stable)
        p.reason = "2D candidate only; depth/force unverified" if stable else "hold camera still"
        return p

class Segmenter:
    def __init__(self, weights, device="cpu", classes=("carton", "black_parcel"), confidence=.6):
        from ultralytics import YOLO
        from pathlib import Path
        if not Path(weights).is_file():
            raise ValueError("Provide a local trained segmentation .pt file; training is not automatic")
        self.model = YOLO(weights, task="segment")
        self.device, self.classes, self.confidence = device, set(classes), confidence
        names = set(self.model.names.values())
        if not self.classes <= names:
            raise ValueError(f"Model classes {names} do not contain {self.classes}; train parcel classes first")

    def predict(self, image):
        import cv2
        result = self.model.predict(source=image, imgsz=320, conf=self.confidence,
                                    device=self.device, verbose=False, retina_masks=True)[0]
        if result.masks is None:
            return None
        raw = result.masks.data.cpu().numpy()
        masks = [cv2.resize(m, image.size, interpolation=cv2.INTER_NEAREST) > .5 for m in raw]
        proposals = []
        for i, mask in enumerate(masks):
            label = result.names[int(result.boxes.cls[i].item())]
            if label not in self.classes:
                continue
            obstacles = np.zeros(mask.shape, dtype=bool)
            for j, other in enumerate(masks):
                if i != j:
                    obstacles |= other
            p = from_mask(mask, label, float(result.boxes.conf[i].item()), obstacles)
            if p:
                proposals.append(p)
        # Prefer confident central candidates; no inference = no proposal, no contour fallback.
        return max(proposals, key=lambda p: p.score-.001*math.hypot(p.u-120,p.v-120), default=None)
