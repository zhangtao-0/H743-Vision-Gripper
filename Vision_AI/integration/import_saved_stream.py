"""Offline-only adapter for existing H743IMG1 recordings. Never opens a port."""
from pathlib import Path
import argparse
import importlib.util
import json
import hashlib
import sys
sys.dont_write_bytecode = True  # importing the legacy decoder must not write into Host

ROOT = Path(__file__).resolve().parents[1]
WIRE = ROOT.parent / "Host/vision_gripper/wire.py"


def import_stream(source, destination, source_kind="unverified"):
    if source_kind not in ("synthetic", "real", "unverified"):
        raise ValueError("Unknown provenance")
    source, destination = Path(source), Path(destination)
    if destination.exists():
        raise ValueError("Choose new output; never overwrite raw data")
    # Reuse protocol validation instead of forking its CRC/format implementation.
    import sys
    spec = importlib.util.spec_from_file_location("h743_existing_wire", WIRE)
    wire = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = wire
    spec.loader.exec_module(wire)
    decoder, records = wire.Decoder(), []
    destination.mkdir(parents=True)
    with source.open("rb") as f:
        while block := f.read(65536):
            for frame in decoder.feed(block):
                stem = f"{len(records):06d}_seq{frame.sequence}"
                image = wire.rgb_image(frame)
                image.save(destination / (stem + ".png"))
                meta = dict(source_kind=source_kind, sequence=frame.sequence, tick_ms=frame.tick, flags=frame.flags,
                            orientation="raw_sensor", coordinate_system="Pixel", width=frame.width, height=frame.height,
                            source_stream_sha256=None, motion_commands_sent=False)
                records.append(meta)
                (destination / (stem + ".json")).write_text(json.dumps(meta, indent=2), encoding="utf8")
    digest = hashlib.sha256()
    with source.open("rb") as f:
        for block in iter(lambda: f.read(65536), b""):
            digest.update(block)
    report = dict(frames=len(records), bad_crc=decoder.bad_crc, discarded_bytes=decoder.discarded,
                  trailing_bytes=len(decoder.buffer), source_stream_sha256=digest.hexdigest(), source_kind=source_kind,
                  protocol_source=str(WIRE), protocol_source_sha256=hashlib.sha256(WIRE.read_bytes()).hexdigest(),
                  serial_opened=False, commands_sent=False)
    (destination / "import_report.json").write_text(json.dumps(report, indent=2), encoding="utf8")
    if not records:
        raise ValueError("No valid H743IMG1 frame; inspect import_report.json")
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--source-kind", choices=("synthetic", "real", "unverified"), default="unverified")
    a = p.parse_args()
    print(json.dumps(import_stream(a.input, a.output, a.source_kind), indent=2))


if __name__ == "__main__":
    main()
