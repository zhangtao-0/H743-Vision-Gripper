"""Read-only USB/camera diagnostics with optional video test; never sends motion."""
import argparse,json,time
from pathlib import Path
from start_viewer import find_port
from wire import Decoder,rgb_image

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument("--port");p.add_argument("--video-test",action="store_true");p.add_argument("--output",type=Path,default=Path(__file__).parent/"diagnostics")
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 import serial
 report={"port":None,"motion_commands_sent":False};decoder=Decoder();frames=[]
 try:
  report["port"]=find_port(a.port)
  with serial.Serial(report["port"],115200,timeout=.08,write_timeout=.5) as com:
   raw=b""
   for cmd in [b"@video status\r\n",b"@status\r\n"]:
    com.write(cmd);time.sleep(.2);raw+=com.read(8192)
   report["status_before"]=raw.decode("ascii",errors="replace")[:6000]
   if a.video_test:
    try:
     for cmd in [b"@video fps 2\r\n",b"@camera live\r\n",b"@video on\r\n"]:com.write(cmd);time.sleep(.15)
     start=time.monotonic()
     while time.monotonic()-start<6:
      frames+=decoder.feed(com.read(min(com.in_waiting or 1,32768)))
     report.update(frames=len(frames),crc_errors=decoder.bad_crc,seconds=round(time.monotonic()-start,3))
     if frames:rgb_image(frames[-1]).save(a.output/"last_frame.png")
    finally:com.write(b"@video off\r\n")
 except Exception as exc:report["error"]=str(exc)
 path=a.output/(time.strftime("usb_%Y%m%d_%H%M%S")+".json");path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
 print(json.dumps(report,ensure_ascii=True,indent=2));print("Saved:",path)
 return int("error" in report or (a.video_test and not frames))
if __name__=="__main__":raise SystemExit(main())
