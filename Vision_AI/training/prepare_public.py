"""Freeze a native-label-preserving TACO subset into a local segmentation version."""
from collections import defaultdict
from pathlib import Path
import hashlib
import json
import math
import random
import shutil

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "Library"
GROUPS = {"cup": set(range(20, 25)), "box": set(range(14, 20)), "bottle": set(range(4, 7))}


def build(version="taco_containers_v1", seed=42):
    if Path(version).name != version or version in (".", ".."):
        raise ValueError("Unsafe dataset version")
    destination = LIB / "04_数据集版本" / version
    if destination.exists():
        raise ValueError("Dataset is immutable: choose a new version")
    downloads = json.loads((LIB / "00_索引与许可/taco_downloads.json").read_text(encoding="utf8"))
    native = json.loads((LIB / "01_来源原件/TACO/annotations.json").read_text(encoding="utf8"))
    categories = {c["id"]: c["name"] for c in native["categories"]}
    grouped = defaultdict(list)
    for record in downloads["records"]:
        if record["status"] == "verified":
            grouped[record["primary_group"]].append(record)
    splits, records, hashes = {}, [], {}
    names = list(GROUPS)
    for group in names:
        items = sorted(grouped[group], key=lambda x: x["image_id"])
        if len(items) < 8:
            raise ValueError(f"Insufficient licensed images in {group}")
        random.Random(seed).shuffle(items)
        count = max(2, len(items) // 5)
        for i, item in enumerate(items):
            split = "test" if i < count else "val" if i < 2 * count else "train"
            image_id = item["image_id"]
            if image_id in splits:
                raise ValueError("The same source image cannot cross groups")
            splits[image_id] = split
            source = ROOT / item["local_path"]
            sha = hashlib.sha256(source.read_bytes()).hexdigest()
            if sha != item["sha256"]:
                raise ValueError("Original changed after ingest")
            if sha in hashes and hashes[sha] != split:
                raise ValueError("Duplicate source content crosses splits")
            hashes[sha] = split
            with Image.open(source) as image:
                width, height = image.size
            lines, objects = [], []
            for annotation in item["source_metadata"]["annotations"]:
                target = next((name for name, ids in GROUPS.items() if annotation["category_id"] in ids), None)
                if target is None:
                    continue
                polygons = annotation.get("segmentation")
                if not isinstance(polygons, list):
                    raise ValueError("RLE needs explicit conversion; never silently drop an object")
                # Multi-component instances require a dedicated representation/converter.
                # This starter version fails rather than turning components into fake independent objects.
                if len(polygons) != 1:
                    raise ValueError(f"Multi-polygon object {annotation['id']}: explicit adaptation required")
                polygon = polygons[0]
                if len(polygon) < 6 or len(polygon) % 2:
                    raise ValueError("Invalid native polygon")
                coords = [float(value) / (width if j % 2 == 0 else height) for j, value in enumerate(polygon)]
                if not all(math.isfinite(x) and 0 <= x <= 1 for x in coords):
                    raise ValueError("Polygon outside image")
                lines.append(str(names.index(target)) + " " + " ".join(f"{x:.8f}" for x in coords))
                objects.append(dict(native_annotation_id=annotation["id"], native_category_id=annotation["category_id"],
                                    native_category=categories[annotation["category_id"]], training_group=target,
                                    tags=[dict(id="semantic:" + target, confidence=1.0, source="source_annotation")]))
            if not lines:
                raise ValueError("Selected image lacks target objects")
            records.append(dict(source=source, image_id=image_id, split=split, sha256=sha, labels=lines, objects=objects,
                                license=item["license"], attribution=item["attribution"], original_url=item["original_url"]))
    # Finish validation before writing any dataset output.
    destination.mkdir(parents=True)
    manifest = []
    for record in records:
        name = f"taco_{record['image_id']:06d}"
        image = destination / "images" / record["split"] / (name + ".jpg")
        label = destination / "labels" / record["split"] / (name + ".txt")
        image.parent.mkdir(parents=True, exist_ok=True)
        label.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(record["source"], image)
        label.write_text("\n".join(record["labels"]) + "\n", encoding="utf8")
        manifest.append({k: v for k, v in record.items() if k not in ("source", "labels")} | dict(
            image=image.relative_to(destination).as_posix(), label=label.relative_to(destination).as_posix(),
            label_sha256=hashlib.sha256(label.read_bytes()).hexdigest(),
            native_source=str(record["source"].relative_to(ROOT))))
    (destination / "data.yaml").write_text("path: " + json.dumps(destination.resolve().as_posix()) + "\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n" + "".join(f"  {i}: {name}\n" for i, name in enumerate(names)), encoding="utf8")
    result = dict(schema="h743.dataset.v1", version=version, seed=seed, classes=names, records=manifest,
                  source_kind="public_real_images", grouping="source_image_id; all instances share split",
                  capture_sessions="unknown in public metadata; perceptual/scene correlation not ruled out",
                  h743_domain_images=0, purpose="bounded pipeline training; not a gripper performance benchmark")
    (destination / "manifest.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    return destination


if __name__ == "__main__":
    print(build())
