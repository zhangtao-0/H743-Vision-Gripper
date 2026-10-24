"""Auto-select H743 native USB CDC and start preview; training/model not required."""
import argparse
from pathlib import Path
import subprocess
import sys

def find_port(requested=None):
    from serial.tools.list_ports import comports
    ports=list(comports())
    if requested:
        if requested.upper() not in {p.device.upper() for p in ports}:
            raise RuntimeError(f"Port {requested} is not present; reconnect H743 USB")
        return requested
    native=[p for p in ports if p.vid==0x0483 and p.pid==0x5740]
    if len(native)==1:return native[0].device
    listing="; ".join(f"{p.device}: {p.description}" for p in ports)
    raise RuntimeError("Expected one H743 CDC (0483:5740); use --port to choose explicitly. Present: "+listing)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port");p.add_argument("--weights");p.add_argument("--device",default="cpu")
    p.add_argument("--send-advice",action="store_true")
    a=p.parse_args()
    try:port=find_port(a.port)
    except Exception as exc:print(str(exc));return 2
    cmd=[sys.executable,str(Path(__file__).with_name("viewer.py")),"--port",port]
    if a.weights:cmd += ["--weights",a.weights,"--device",a.device]
    if a.send_advice:cmd += ["--send-advice"]
    print(f"H743 USB CDC: {port}. Close XCAM/serial tools holding this same COM port.")
    print("Preview starts video automatically. No model is required for preview/capture.")
    return subprocess.call(cmd)
if __name__=="__main__":raise SystemExit(main())
