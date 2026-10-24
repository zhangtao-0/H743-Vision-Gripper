"""Versioned RGB565 frame stream. Text outside frames is ignored, not pixels."""
from dataclasses import dataclass
import struct
import zlib
MAGIC = b"H743IMG1"
HEADER = struct.Struct("<8sBBBBIIHHIIII")
MAX_PAYLOAD = 240 * 240 * 2

@dataclass(frozen=True)
class Frame:
    sequence: int
    tick: int
    width: int
    height: int
    flags: int
    pixels: bytes

def encode(frame):
    fields = (MAGIC, 1, 1, frame.flags, HEADER.size, frame.sequence, frame.tick,
              frame.width, frame.height, len(frame.pixels), zlib.crc32(frame.pixels), 0, 0)
    head = HEADER.pack(*fields)
    return head[:-4] + struct.pack("<I", zlib.crc32(head[:-4])) + frame.pixels

class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_crc = 0
        self.frames = 0
        self.discarded = 0

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while True:
            start = self.buffer.find(MAGIC)
            if start < 0:
                remove = max(0, len(self.buffer) - len(MAGIC) + 1)
                self.discarded += remove
                del self.buffer[:remove]
                break
            if start:
                self.discarded += start
                del self.buffer[:start]
            if len(self.buffer) < HEADER.size:
                break
            head = bytes(self.buffer[:HEADER.size])
            magic, version, fmt, flags, length, seq, tick, w, h, size, crc, reserved, hc = HEADER.unpack(head)
            if (zlib.crc32(head[:-4]) != hc or version != 1 or fmt != 1 or length != 40
                    or w != 240 or h != 240 or size != w * h * 2 or size > MAX_PAYLOAD
                    or flags > 15 or reserved != 0):
                self.bad_crc += 1
                del self.buffer[0]
                continue
            total = HEADER.size + size
            if len(self.buffer) < total:
                break
            payload = bytes(self.buffer[HEADER.size:total])
            if zlib.crc32(payload) != crc:
                self.bad_crc += 1
                del self.buffer[0]  # resync even when a truncated frame contains the next header
                continue
            result.append(Frame(seq, tick, w, h, flags, payload))
            self.frames += 1
            del self.buffer[:total]
        return result

def rgb_image(frame):
    import numpy as np
    from PIL import Image
    p = np.frombuffer(frame.pixels, dtype="<u2").reshape(frame.height, frame.width)
    r = ((p >> 11) & 31).astype(np.uint16) * 255 // 31
    g = ((p >> 5) & 63).astype(np.uint16) * 255 // 63
    b = (p & 31).astype(np.uint16) * 255 // 31
    return Image.fromarray(np.stack((r, g, b), axis=-1).astype(np.uint8))

def displayed(image, flags):
    from PIL import Image
    image = image.rotate(-90 * (flags & 3), expand=True)
    if flags & 4:
        image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if flags & 8:
        image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    return image
