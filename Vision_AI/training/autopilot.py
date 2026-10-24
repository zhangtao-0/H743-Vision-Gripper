"""One command checks, freezes, trains, evaluates and registers a challenger.

No scheduler, uploads, arbitrary package installs, model deployment or hardware I/O.
"""
from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "Library"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run():
    policy = json.loads((ROOT / "configs/autopilot.json").read_text(encoding="utf8"))
    manifest_path = (ROOT / policy["dataset_manifest"]).resolve()
    if (LIB / "04_数据集版本").resolve() not in manifest_path.parents:
        raise ValueError("Dataset manifest must remain in the versioned local library")
    if not manifest_path.exists():
        from prepare_public import build
        build()
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    dataset = manifest_path.parent
    by_hash, counts = {}, {}
    for row in manifest["records"]:
        image, label = dataset / row["image"], dataset / row["label"]
        if digest(image) != row["sha256"] or digest(label) != row["label_sha256"]:
            raise ValueError("Frozen dataset content changed; do not train")
        if row["sha256"] in by_hash and by_hash[row["sha256"]] != row["split"]:
            raise ValueError("Train/test duplicate leakage")
        by_hash[row["sha256"]] = row["split"]
        counts[row["split"]] = counts.get(row["split"], 0) + 1
    source = json.loads((LIB / "05_模型注册/pretrained/source.json").read_text(encoding="utf8"))
    if source["status"] != "downloaded":
        raise ValueError("No verified local pretrained weight")
    weights = ROOT / source["local_path"]
    if digest(weights) != source["sha256"]:
        raise ValueError("Pretrained weights changed")
    recipe = policy["budgets"]
    identity = dict(manifest_sha256=digest(manifest_path), yaml_sha256=digest(dataset / "data.yaml"),
                    weights_sha256=digest(weights), recipe=recipe,
                    worker_sha256=digest(ROOT / "training/train_worker.py"))
    fingerprint = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
    registry_path = LIB / "05_模型注册/registry.json"
    registry = json.loads(registry_path.read_text(encoding="utf8")) if registry_path.exists() else dict(champion=None, runs=[])
    previous = next((r for r in registry["runs"] if r["fingerprint"] == fingerprint and r["status"] == "completed"), None)
    if previous:
        print(json.dumps(dict(status="up_to_date", run=previous["run_id"], reason="same data/weights/recipe: no redundant retraining")))
        return
    interpreter = ROOT.parent / "Host/vision_gripper/.venv/Scripts/python.exe"
    if not interpreter.is_file():
        raise RuntimeError("Missing existing CPU PyTorch environment; report dependency need instead of auto-install")
    run_id = "auto_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    output = ROOT / "experiments" / run_id
    output.mkdir()
    job = dict(data=str(dataset / "data.yaml"), weights=str(weights), output=str(output),
               threads=recipe["cpu_threads"], epochs=recipe["epochs"], batch=recipe["batch"], imgsz=recipe["imgsz"],
               identity=identity, split_counts=counts, fingerprint=fingerprint)
    job_path = output / "job.json"
    job_path.write_text(json.dumps(job, ensure_ascii=False, indent=2), encoding="utf8")
    command = [str(interpreter), str(ROOT / "training/train_worker.py"), "--job", str(job_path)]
    status = dict(run_id=run_id, fingerprint=fingerprint, status="running", command=command, output=output.relative_to(ROOT).as_posix())
    registry["runs"].append(status)
    registry_path.write_text(json.dumps(registry, ensure_ascii=False, indent=2), encoding="utf8")
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", YOLO_AUTOINSTALL="false", OMP_NUM_THREADS=str(recipe["cpu_threads"]))
    try:
        with (output / "training.log").open("w", encoding="utf8") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env, cwd=ROOT,
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            try:
                code = process.wait(timeout=recipe["max_minutes_per_job"] * 60)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise RuntimeError("Job time budget exceeded; own worker terminated")
        if code:
            raise RuntimeError(f"Training failed with exit {code}; inspect training.log")
        result = json.loads((output / "metrics.json").read_text(encoding="utf8"))
        status.update(status="completed", metrics=result, checkpoint_sha256=digest(result["checkpoint"]))
        missing = []
        if manifest["h743_domain_images"] < policy["promotion"]["minimum_real_h743_test_images"]:
            missing.append("no_heldout_H743_domain_benchmark")
        if result["test"].get("metrics/mAP50(M)", 0) < policy["promotion"]["minimum_mask_map50"]:
            missing.append("mask_map50_below_gate")
        if result["test"].get("metrics/mAP50-95(M)", 0) < policy["promotion"]["minimum_mask_map50_95"]:
            missing.append("mask_map50_95_below_gate")
        status["promotion"] = dict(status="challenger_only", blocked_reasons=missing or ["requires_baseline_comparison"],
                                   hardware_deployed=False)
    except Exception as error:
        status.update(status="failed", error=str(error))
        raise
    finally:
        registry_path.write_text(json.dumps(registry, ensure_ascii=False, indent=2), encoding="utf8")
        (output / "run_status.json").write_text(json.dumps(status, ensure_ascii=False, indent=2), encoding="utf8")
    print(json.dumps(dict(run=run_id, status=status["status"], promotion=status["promotion"]), ensure_ascii=False))


if __name__ == "__main__":
    # OS-backed lock: released on process exit, unlike stale PID files.
    lockpath = ROOT / "experiments/autopilot.lock"
    lockpath.parent.mkdir(exist_ok=True)
    with lockpath.open("a+b") as lock:
        if lock.tell() == 0:
            lock.write(b"0")
            lock.flush()
        lock.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            run()
        finally:
            lock.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock, fcntl.LOCK_UN)

