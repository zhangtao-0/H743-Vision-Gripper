"""Stable teaching entry point. No installs, model downloads, cameras or serial I/O."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def interpreter():
    candidates = [ROOT / ".venv/Scripts/python.exe", ROOT.parent / "Host/vision_gripper/.venv/Scripts/python.exe"]
    for python in candidates:
        if python.is_file():
            result = subprocess.run([str(python), "-c", "import cv2,numpy,PIL"], capture_output=True)
            if result.returncode == 0:
                return python
    raise RuntimeError("No usable baseline environment. Review Docs/05_安装审批清单.md; this tool never installs automatically.")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="action", required=True)
    sub.add_parser("test")
    sub.add_parser("demo")
    photo = sub.add_parser("photo")
    photo.add_argument("--input", required=True, type=Path)
    photo.add_argument("--output", required=True, type=Path)
    photo.add_argument("--config", type=Path)
    args = p.parse_args()
    python = interpreter()
    print("Read-only execution environment:", python, flush=True)
    if args.action == "test":
        cmd = [str(python), "-m", "unittest", "discover", "-s", str(ROOT / "tests"), "-v"]
    elif args.action == "demo":
        cmd = [str(python), str(ROOT / "tools/run_baseline.py")]
    else:
        cmd = [str(python), str(ROOT / "src/baseline.py"), "--input", str(args.input.resolve()), "--output", str(args.output.resolve()), "--source-kind", "real"]
        if args.config:
            cmd += ["--config", str(args.config.resolve())]
    return subprocess.call(cmd, cwd=ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
