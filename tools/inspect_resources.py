"""Decode original GRED resources without third-party Python dependencies."""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


def png(path, w, h, pixels):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    raw = b''.join(b'\0' + bytes(pixels[y*w*3:(y+1)*w*3]) for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def decode():
    lib = (ROOT / 'original/ALENKA/ALLE.LIB').read_bytes()
    fld = (ROOT / 'original/ALENKA/ALLE.FLD').read_bytes()
    palette = [tuple((0, 85, 170, 255)[c] for c in struct.unpack_from('<3H', lib, 18+i*6)) for i in range(17)]
    stride, height = struct.unpack_from('<HH', lib, 8)  # tile width, height
    width_bytes = struct.unpack_from('<H', lib, 6)[0] + 1
    lines = struct.unpack_from('<H', lib, 4)[0] + 1
    plane = width_bytes * lines
    pixels = bytearray()
    indices = []
    for y in range(lines):
        for x in range(width_bytes*8):
            color = sum(((lib[256 + (3-p)*plane + y*width_bytes+x//8] >> (7-x%8)) & 1) << p for p in range(4))
            indices.append(color)
            pixels.extend(palette[color])
    out = ROOT / 'diagnostics/inspection'
    out.mkdir(parents=True, exist_ok=True)
    png(out / 'atlas.png', width_bytes*8, lines, pixels)
    fw, fh = struct.unpack_from('<HH', fld, 8)
    cells = struct.unpack_from(f'<{fw*fh}H', fld, 32)
    image = bytearray(fw*8*fh*8*3)
    for cy in range(fh):
        for cx in range(fw):
            ptr = cells[cy*fw+cx]-16000
            tx, ty = ptr % width_bytes * 8, ptr // width_bytes
            if not (0 <= ty <= lines-8 and tx <= width_bytes*8-8):
                raise ValueError((cx,cy,ptr))
            for y in range(8):
                for x in range(8):
                    dst = ((cy*8+y)*fw*8+cx*8+x)*3
                    src = ((ty+y)*width_bytes*8+tx+x)*3
                    image[dst:dst+3] = pixels[src:src+3]
    png(out / 'fields.png', fw*8, fh*8, image)
    png(out / 'intro.png', 640, 350, image[:640*350*3])
    print('Atlas:', width_bytes*8, lines, 'tiles:', stride, height)
    print('Field:', fw, fh, 'palette:', palette)


if __name__ == '__main__':
    decode()
