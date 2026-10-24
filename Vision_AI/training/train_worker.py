"""A bounded local training worker, invoked by autopilot after source validation."""
from pathlib import Path
import argparse
import json
import math
import os
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
os.environ["YOLO_CONFIG_DIR"] = str(ROOT / "experiments/ultralytics_local_config")
os.environ["YOLO_AUTOINSTALL"] = "false"
os.environ["WANDB_MODE"] = "disabled"
os.environ["COMET_MODE"] = "DISABLED"


def metrics(result):
    values = {str(k): float(v) for k, v in result.results_dict.items()}
    if not all(math.isfinite(x) for x in values.values()):
        raise ValueError("Non-finite metrics cannot be registered as success")
    return values


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--job", type=Path, required=True)
    a = p.parse_args()
    job = json.loads(a.job.read_text(encoding="utf8"))
    import torch
    import ultralytics
    from ultralytics import YOLO
    torch.set_num_threads(job["threads"])
    torch.set_num_interop_threads(1)
    output = Path(job["output"])
    output.mkdir(parents=True, exist_ok=True)
    (output / "environment.json").write_text(json.dumps(dict(python=sys.version, interpreter=sys.executable,
        torch=torch.__version__, ultralytics=ultralytics.__version__, cuda_available=torch.cuda.is_available(),
        device="cpu", num_threads=torch.get_num_threads()), indent=2), encoding="utf8")
    model = YOLO(job["weights"], task="segment")
    result = model.train(data=job["data"], epochs=job["epochs"], imgsz=job["imgsz"], batch=job["batch"],
        device="cpu", workers=0, seed=42, deterministic=True, project=str(output), name="train", exist_ok=False,
        cache=False, amp=False, plots=False, save=True, val=True, optimizer="SGD", lr0=.001, momentum=.9,
        mosaic=0, close_mosaic=0, verbose=False)
    best = Path(model.trainer.best)
    if not best.is_file():
        raise RuntimeError("Training returned without a saved best checkpoint")
    # Test is reporting only, never used to choose an epoch or mutate a trial.
    frozen = YOLO(str(best), task="segment")
    test = frozen.val(data=job["data"], split="test", imgsz=job["imgsz"], batch=job["batch"],
        device="cpu", workers=0, plots=False, project=str(output), name="heldout_test", exist_ok=False,
        save_json=True, verbose=False)
    predictions = frozen.predict(source=str(Path(job["data"]).parent / "images/test"), imgsz=job["imgsz"],
        device="cpu", conf=.25, save=False, verbose=False)
    raw = [dict(image=Path(pred.path).name, detections=pred.summary(normalize=True)) for pred in predictions]
    (output / "test_predictions.json").write_text(json.dumps(raw, ensure_ascii=False, indent=2), encoding="utf8")
    (output / "metrics.json").write_text(json.dumps(dict(validation=metrics(result), test=metrics(test),
        speed_ms=test.speed, checkpoint=str(best), purpose="pipeline_smoke_only", hardware_deployed=False),
        ensure_ascii=False, indent=2, allow_nan=False), encoding="utf8")


if __name__ == "__main__":
    main()
