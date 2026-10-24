"""USB viewer/capture + optional learned segmentation; never sends motion commands."""
import argparse
from dataclasses import asdict
from pathlib import Path
import json
import queue
import threading
import time
import uuid
from wire import Decoder, rgb_image, displayed


def latest(q, item):
    try:
        q.put_nowait(item)
    except queue.Full:
        try:
            q.get_nowait()
        except queue.Empty:
            pass
        try:
            q.put_nowait(item)
        except queue.Full:
            pass


class SerialLink(threading.Thread):
    def __init__(self, port, frames, events, fps):
        super().__init__(daemon=True)
        self.port, self.frames, self.events, self.fps = port, frames, events, fps
        self.commands = queue.Queue(12)
        self.stop = threading.Event()
        self.decoder = Decoder()
        self.received_bytes = 0

    def command(self, command):
        try:
            self.commands.put_nowait(command)
            return True
        except queue.Full:
            return False

    def run(self):
        import serial
        try:
            with serial.Serial(self.port, 115200, timeout=.03, write_timeout=.2) as port:
                for command in (f"@video fps {self.fps}", "@camera live", "@video on"):
                    self.command(command)
                sent = 0
                while not self.stop.is_set():
                    now = time.monotonic()
                    if now-sent >= .12:
                        try:
                            command = self.commands.get_nowait()
                        except queue.Empty:
                            command = None
                        if command:
                            port.write((command+"\r\n").encode("ascii"))
                            sent = now
                    data = port.read(min(port.in_waiting or 1, 16384))
                    self.received_bytes += len(data)
                    for frame in self.decoder.feed(data):
                        latest(self.frames, (frame, time.monotonic()))
                # Best effort only; disconnect/crash is handled by MCU timeout and advice expiry.
                port.write(b"@video off\r\n")
        except Exception as exc:
            self.events.put("USB error: " + str(exc))


class Inference(threading.Thread):
    def __init__(self, args, incoming, outgoing, events):
        super().__init__(daemon=True)
        self.args, self.incoming, self.outgoing, self.events = args, incoming, outgoing, events
        self.stop = threading.Event()

    def run(self):
        try:
            from grasp import Segmenter, Stability
            model = Segmenter(self.args.weights, self.args.device, tuple(self.args.classes.split(",")))
            stable = Stability(4)
            self.events.put("Model ready; segmentation + geometric 2D grasp only")
            while not self.stop.is_set():
                try:
                    frame, received = self.incoming.get(timeout=.2)
                except queue.Empty:
                    continue
                image = rgb_image(frame)
                p = stable.update(model.predict(image))
                if time.monotonic()-received > 1.2:
                    stable.reset()
                    p = None
                latest(self.outgoing, (frame, received, image, p))
        except Exception as exc:
            self.events.put("Inference disabled: " + str(exc))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="H743 USB CDC COM port, NOT the UART1 port occupied by VOFA")
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--fps", type=int, choices=range(1, 6), default=2)
    parser.add_argument("--weights", help="local trained segmentation best.pt; omit for preview/capture")
    parser.add_argument("--device", default="cpu", help="cpu or GPU index, e.g. 0")
    parser.add_argument("--classes", default="carton,black_parcel")
    parser.add_argument("--output", type=Path, default=Path(__file__).parent / "data/raw")
    parser.add_argument("--send-advice", action="store_true", help="explicitly send stable pixel advice; NEVER motion")
    args = parser.parse_args()
    if args.list_ports:
        from serial.tools.list_ports import comports
        for port in comports():
            print(port.device, port.description, port.hwid)
        return
    if not args.port:
        parser.error("--port is required (use --list-ports first)")
    import tkinter as tk
    from PIL import ImageTk, ImageDraw
    frames, infer_in, infer_out, events = queue.Queue(1), queue.Queue(1), queue.Queue(1), queue.Queue()
    link = SerialLink(args.port, frames, events, args.fps)
    worker = Inference(args, infer_in, infer_out, events) if args.weights else None
    window = tk.Tk()
    window.title("H743 parcel vision - pixel advice / NO MOTION")
    canvas = tk.Label(window)
    canvas.pack()
    text = tk.StringVar(value="Connecting USB; VOFA can keep UART1 open separately")
    tk.Label(window, textvariable=text, wraplength=720).pack()
    notice = tk.StringVar(value="No learned model loaded" if not args.weights else "Loading model...")
    tk.Label(window, textvariable=notice, wraplength=720).pack()
    session = time.strftime("%Y%m%d_%H%M%S_") + uuid.uuid4().hex[:6]
    output = args.output/session
    state = {"current": None, "last_seen": 0., "last_advice": None, "cleared": True,
             "error": "", "inference_ready": False, "started": time.monotonic(), "warned": False, "pose": None, "last_display": 0.}

    def save():
        if not state["current"]:
            return
        frame, received, image, proposal = state["current"]
        if time.monotonic()-received > 1.5:
            text.set("No fresh frame to save")
            return
        output.mkdir(parents=True, exist_ok=True)
        stem = f"{frame.sequence:010d}_{time.time_ns()}"
        image.save(output/(stem+".png"))  # always raw orientation; display transforms never contaminate labels
        meta = dict(session=session, sequence=frame.sequence, tick=frame.tick, flags=frame.flags,
                    orientation="raw_sensor", model=args.weights, proposal=asdict(proposal) if proposal else None)
        (output/(stem+".json")).write_text(json.dumps(meta, indent=2), encoding="utf-8")
        text.set("Saved raw RGB PNG + metadata: " + stem)

    tk.Button(window, text="Save raw image [S]", command=save).pack(side="left")
    tk.Button(window, text="Clear advisory", command=lambda: link.command("@vision clear")).pack(side="left")
    window.bind("s", lambda _: save())

    def show(frame, received, image, p):
        annotated = image.copy()
        draw = ImageDraw.Draw(annotated)
        draw.line((110,120,130,120), fill="white")
        draw.line((120,110,120,130), fill="white")
        if p:
            color = "lime" if p.stable else "orange"
            draw.line((*p.endpoints[0], *p.endpoints[1]), fill=color, width=2)
            draw.ellipse((p.u-3,p.v-3,p.u+3,p.v+3), outline=color, width=2)
            status = f"{p.label} score={p.score:.2f} raw=({p.u:.0f},{p.v:.0f}) offset=({p.u-120:+.0f},{p.v-120:+.0f})px angle={p.angle:.1f} width={p.width:.0f}px {p.reason}"
        else:
            status = "Preview/capture only" if not args.weights else "No accepted grasp candidate"
        visible = displayed(annotated, frame.flags).resize((600,600))
        photo = ImageTk.PhotoImage(visible)
        canvas.configure(image=photo)
        canvas.image = photo
        state["current"] = (frame, received, image, p)
        state["last_display"] = received
        text.set(f"Frame {frame.sequence} CRC errors={link.decoder.bad_crc}; {status}; NO MOTION")
        if args.send_advice:
            fresh = time.monotonic()-received < 1.2
            if p and p.stable and fresh and state["last_advice"] != frame.sequence:
                width = min(240,max(1,round(p.width)))
                command = f"@vision target {frame.sequence} {round(p.u)} {round(p.v)} {max(-900,min(899,round(p.angle*10)))} {width} {round(p.score*1000)}"
                if link.command(command):
                    state["last_advice"], state["cleared"] = frame.sequence, False
            elif (not p or not p.stable or not fresh) and not state["cleared"]:
                link.command("@vision clear")
                state["cleared"] = True

    def poll():
        if not state["last_seen"] and not state["warned"] and time.monotonic()-state["started"] > 8:
            state["warned"]=True
            notice.set(f"No image frame yet; RX={link.received_bytes} bytes. Check COM/firmware, @video status, camera ready. Close XCAM on this port.")
        try:
            frame, received = frames.get_nowait()
            pose = frame.flags
            if state["pose"] is not None and pose != state["pose"]:
                # Raw coordinates stay unchanged; transforms affect display only.
                link.command("@vision clear")
            state["pose"] = pose
            state["last_seen"] = received
            latest(infer_in, (frame, received))
            if state["inference_ready"] and time.monotonic()-state["last_display"] > 1.5:
                state["inference_ready"] = False
            if not state["inference_ready"]:
                show(frame, received, rgb_image(frame), None)
        except queue.Empty:
            pass
        try:
            frame, received, image, p = infer_out.get_nowait()
            if time.monotonic()-received <= 1.2:
                state["inference_ready"] = True
                show(frame, received, image, p)
            else:
                state["inference_ready"] = False
                link.command("@vision clear")
        except queue.Empty:
            pass
        try:
            message = events.get_nowait()
            state["error"] = message
            notice.set(message)
            if message.startswith("Inference disabled:"):
                state["inference_ready"] = False
                link.command("@vision clear")
        except queue.Empty:
            pass
        if state["last_seen"] and time.monotonic()-state["last_seen"] > 1.5:
            text.set("STALE: no fresh USB frame; advisory expired; check cable/camera/video status")
            if not state["cleared"]:
                link.command("@vision clear")
                state["cleared"] = True
        window.after(25, poll)

    def close():
        link.stop.set()
        if worker:
            worker.stop.set()
        link.join(timeout=.8)
        window.destroy()

    window.protocol("WM_DELETE_WINDOW", close)
    link.start()
    if worker:
        worker.start()
    poll()
    window.mainloop()

if __name__ == "__main__":
    main()
