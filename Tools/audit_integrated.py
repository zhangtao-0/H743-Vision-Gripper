"""Produce integration evidence from the actual IAR logs, maps, files and tests."""
from pathlib import Path
import collections
import hashlib
import json
import re
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Docs/整合交付"
CONFIGS = ["H743_VisionGripper", "H743_VisionGripper_OV2640", "H743_VisionGripper_NoCamera"]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def save(name, data):
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

def main():
    builds = []
    for config in CONFIGS:
        log_path = OUT / (config + ".log")
        log = log_path.read_text(encoding="utf-8-sig", errors="replace")
        errors = re.findall(r"Total number of errors:\s*(\d+)", log)
        warnings = re.findall(r"Total number of warnings:\s*(\d+)", log)
        assert errors and warnings and int(errors[-1]) == 0 and "Build succeeded" in log, config
        firmware = ROOT / f"project/EWARM/{config}/Exe/H743_VisionGripper.hex"
        map_path = ROOT / f"project/EWARM/{config}/List/H743_VisionGripper.map"
        builds.append({"configuration": config, "errors": int(errors[-1]), "warnings": int(warnings[-1]),
                       "hex": firmware.relative_to(ROOT).as_posix(), "hex_sha256": sha(firmware),
                       "map_sha256": sha(map_path), "log": log_path.relative_to(ROOT).as_posix(),
                       "memory_summary": [line.strip() for line in log.splitlines() if "bytes of" in line and "read" in line]})
    origin = json.loads((OUT / "厂商来源清单.json").read_text(encoding="utf-8"))
    for item in origin:
        assert sha(ROOT / item["source"]) == item["original_sha256"], item["source"]
        item["adapted_sha256"] = sha(ROOT / item["target"])
    save("厂商来源清单.json", origin)
    project_xml = ET.parse(ROOT / "project/EWARM/H743_VisionGripper.ewp").getroot()
    registered = []
    for entry in project_xml.findall(".//file/name"):
        path = Path(entry.text.replace("$PROJ_DIR$", str(ROOT / "project/EWARM")).replace("\\", "/")).resolve()
        assert path.is_file(), str(path)
        registered.append(path)
    assert not [p for p, count in collections.Counter(registered).items() if count > 1], "Duplicate build sources"
    tests = []
    for rel in ["Docs/适配规划/05_调查证据/P2主机测试.json", "Docs/适配规划/05_调查证据/P3主机测试.json", "Docs/整合交付/整合主机测试.json", "Docs/故障排查/20261021_花屏与舵机离线/摄像头主机测试.json", "Docs/电机控制/主机测试.json", "Docs/屏幕交互/主机状态机测试.json", "Docs/屏幕交互/主机开机配置测试.json", "Docs/屏幕交互/主机配置Flash模型测试.json"]:
        test = json.loads((ROOT / rel).read_text(encoding="utf-8"))
        assert test["exit_code"] == 0, rel
        tests.append({"evidence": rel, "stdout": test["stdout"], "scope": test["scope"]})
    with zipfile.ZipFile(ROOT / "Backups/project_pre_full_integration_20261021.zip") as archive:
        before = {name: hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()}
    changes = []
    inventory = {}
    for path in (ROOT / "project").rglob("*"):
        if not path.is_file() or path.suffix not in (".c", ".h", ".ewp", ".ioc", ".icf", ".s"):
            continue
        if any(part in path.parts for part in ("Obj", "List", "BrowseInfo", "Exe")):
            continue
        rel = path.relative_to(ROOT).as_posix()
        digest = sha(path)
        inventory[rel] = digest
        if before.get(rel) != digest:
            changes.append({"path": rel, "action": "modified" if rel in before else "added", "before": before.get(rel), "after": digest})
    save("文件变更清单.json", changes)
    save("当前源码哈希.json", inventory)
    map_text = (ROOT / "project/EWARM/H743_VisionGripper/List/H743_VisionGripper.map").read_text(encoding="utf-8", errors="replace")
    dma_lines = [line.strip() for line in map_text.splitlines() if ".camera_dma" in line]
    assert any("0x2400'0000" in line and "uninit" in line for line in dma_lines), "Camera must be in AXI SRAM"
    assert "0x0817FFFF" in (ROOT / "project/EWARM/stm32h743xx_flash.icf").read_text(encoding="utf-8"), "Boot profile and mechanical-limit regions must be excluded"
    field_path = ROOT / "Docs/故障排查/20261021_花屏与舵机离线/现场验证.json"
    field = json.loads(field_path.read_text(encoding="utf-8")) if field_path.exists() else {"flashed": False}
    current_flashed = bool(field.get("flashed") and field.get("verified") and field.get("hex_sha256") == builds[0]["hex_sha256"])
    hardware = field if current_flashed else {"current_build": "not flashed or hardware validated", "prior_build_evidence": field}
    save("构建与验证汇总.json", {"builds": builds, "tests": tests, "imported_files": len(origin),
        "vendor_originals_unchanged": True, "registered_files": len(registered), "changed_files": len(changes),
        "hal_version": "1.11.6", "cube_package_declared": "STM32Cube FW_H7 V1.13.0",
        "camera_map_evidence": dma_lines, "hardware_validation": hardware, "flashed": current_flashed,
        "autonomous_visual_grasping": "not implemented; integrated camera acquisition and preview only"})
    broken = []
    for base in [ROOT / "Docs/整合交付", ROOT / "Docs/适配规划", ROOT / "Docs/电机控制"]:
        for path in base.rglob("*.md"):
            for target in re.findall(r"\]\(([^)]+)\)", path.read_text(encoding="utf-8-sig")):
                if not target.startswith(("http:", "https:", "#")) and not (path.parent / target).resolve().exists():
                    broken.append([path.relative_to(ROOT).as_posix(), target])
    assert not broken, broken
    print(json.dumps({"builds": [(b["configuration"], b["errors"], b["warnings"]) for b in builds],
                      "imported_files": len(origin), "changed_files": len(changes), "broken_links": broken}, ensure_ascii=False))

if __name__ == "__main__":
    main()
