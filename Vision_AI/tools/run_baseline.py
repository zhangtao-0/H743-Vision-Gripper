"""Reproduce original example and controlled failures; uses no serial/network/model."""
from pathlib import Path
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys

import cv2
import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
from baseline import Config, run, write_image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--run-id", default=datetime.datetime.now().strftime("baseline_%Y%m%d_%H%M%S_%f"))
    args = p.parse_args()
    if not args.run_id or Path(args.run_id).name != args.run_id or args.run_id in (".", ".."):
        p.error("run-id must be a directory name")
    folder = ROOT / "experiments" / args.run_id
    folder.mkdir(exist_ok=False)
    original = ROOT / "Resources/教学包原件/03_最小可运行演示"
    # Original CLI runs on a disposable work copy. The source snapshot stays intact.
    work = folder / "original_run"
    shutil.copytree(original, work, ignore=shutil.ignore_patterns("outputs", "__pycache__"))
    logs = []
    for cmd in ([sys.executable, "src/synthetic_demo.py"], [sys.executable, "src/shape_baseline.py"], [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"]):
        r = subprocess.run(cmd, cwd=work, env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"), capture_output=True, text=True, encoding="utf8", errors="replace")
        logs.append(dict(command=cmd, cwd=str(work), exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr))
        (folder / "original_commands.json").write_text(json.dumps(logs, ensure_ascii=False, indent=2), encoding="utf8")
        if r.returncode:
            raise RuntimeError("Original reproduction failed; inspect original_commands.json")
    spec = importlib.util.spec_from_file_location("synthetic_demo", original / "src/synthetic_demo.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    normal = module.create_demo_image()
    dim = (normal.astype(float) * .55).astype(np.uint8)
    over = np.full_like(normal, 250)
    occluded = normal.copy()
    cv2.rectangle(occluded, (100, 50), (160, 215), (245, 245, 245), -1)
    dark = np.full_like(normal, 35)
    cv2.rectangle(dark, (240, 160), (390, 270), (210, 210, 210), -1)
    cases = [
        ("normal", normal, Config(), "reference_synthetic", 3),
        ("dim_background_fixed_threshold", dim, Config(), "miss_background_merges_with_foreground", 3),
        ("dim_background_threshold_retuned", dim, Config(threshold=100), "retuned_on_same_input_not_general_accuracy", 3),
        ("overexposed", over, Config(), "miss_exposure_information_lost", 3),
        ("occluded_circle", occluded, Config(), "wrong_shape_due_to_occlusion", 3),
        ("dark_background_wrong_polarity", dark, Config(), "miss_threshold_polarity", 1),
        ("dark_background_light_mode", dark, Config(mode="gray_light", threshold=100), "corrected_polarity", 1),
        ("hsv_dark_foreground", normal, Config(mode="hsv"), "hsv_alternative_same_scene", 3),
    ]
    records = []
    for name, image, cfg, issue, expected in cases:
        path = folder / "inputs" / (name + ".png")
        write_image(path, image)
        result = run(path, folder / name, cfg, "synthetic")
        records.append(dict(case=name, source_kind="synthetic", expected_objects=expected, detected_objects=len(result["objects"]),
                            observed_shapes=[o["shape_hint"] for o in result["objects"]], issue=issue,
                            output=str((folder / name).relative_to(ROOT)), image_sha256=result["image_sha256"]))
    (folder / "case_results.json").write_text(json.dumps(records, indent=2), encoding="utf8")
    hashes = {p.relative_to(folder).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.rglob("*") if p.is_file()}
    (folder / "artifacts_sha256.json").write_text(json.dumps(hashes, indent=2), encoding="utf8")
    print(json.dumps(dict(run=str(folder), cases=records), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
