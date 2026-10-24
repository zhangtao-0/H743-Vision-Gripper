"""Bounded, attributable public-data ingest. No arbitrary crawling or uploads."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import time
import urllib.request

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "Library"


def fetch(url, destination, limit):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        data = destination.read_bytes()
        if len(data) > limit:
            raise ValueError("Existing file exceeds limit")
        return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), cached=True)
    request = urllib.request.Request(url, headers={"User-Agent": "H743-Vision-Library/1.0"})
    with urllib.request.urlopen(request, timeout=35) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError("Download exceeds per-file limit")
    temp = destination.with_suffix(destination.suffix + ".partial")
    temp.write_bytes(data)
    temp.replace(destination)
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), cached=False)


def main():
    selection = json.loads((LIB / "00_索引与许可/taco_selection.json").read_text(encoding="utf8"))
    records = []

    def one(item):
        image = item["image"]
        relative = Path(image["file_name"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Unsafe upstream image path")
        dest = LIB / "01_来源原件/TACO/images" / relative
        record = dict(image_id=image["id"], file_name=relative.as_posix(), primary_group=item["primary_group"],
                      original_url=image["flickr_url"], license=item["image_license"],
                      license_basis="Official TACO terms: missing image license defaults to CC BY4.0",
                      attribution="TACO contributors; Pedro F. Proenca and Pedro Simoes; retain source image URL",
                      source_metadata=item, local_path=dest.relative_to(ROOT).as_posix())
        for attempt in range(2):
            try:
                record.update(fetch(image["flickr_url"], dest, 10 * 1024 * 1024))
                with Image.open(dest) as im:
                    im.verify()
                with Image.open(dest) as im:
                    if im.size != (image["width"], image["height"]):
                        raise ValueError(f"Image/annotation dimensions differ: {im.size}")
                    if im.getexif().get(274, 1) not in (None, 1):
                        raise ValueError("EXIF rotation needs explicit polygon adaptation")
                record["status"] = "verified"
                return record
            except Exception as error:
                record["error"] = str(error)
                if attempt == 0:
                    time.sleep(.5)
        record["status"] = "failed"
        return record

    with ThreadPoolExecutor(max_workers=3) as pool:
        for record in pool.map(one, selection["selected"]):
            records.append(record)
            report = dict(source="TACO", license_url="http://tacodataset.org/", records=records,
                          bytes_received=sum(r.get("bytes", 0) for r in records),
                          verified=sum(r["status"] == "verified" for r in records))
            (LIB / "00_索引与许可/taco_downloads.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf8")
            print(record["image_id"], record["status"], flush=True)
    # Explicit local weights; later training is not allowed to resolve arbitrary model names online.
    url = "https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n-seg.pt"
    dest = LIB / "05_模型注册/pretrained/yolo26n-seg.pt"
    try:
        info = dict(url=url, release="v8.4.0", license="Ultralytics AGPL-3.0 / enterprise; review model and upstream dataset terms",
                    local_path=dest.relative_to(ROOT).as_posix(), **fetch(url, dest, 32 * 1024 * 1024), status="downloaded")
    except Exception as error:
        info = dict(url=url, error=str(error), status="failed")
    (LIB / "05_模型注册/pretrained/source.json").write_text(json.dumps(info, ensure_ascii=False, indent=2), encoding="utf8")
    print("images:", report["verified"], "bytes:", report["bytes_received"], "weights:", info["status"])


if __name__ == "__main__":
    main()
