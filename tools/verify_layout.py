"""Check SDL BMP captures against independent native DOS framebuffer, pixel by pixel."""
from pathlib import Path
import struct
import sys
from frame_storage import read_frame


def verify(reference, bitmap):
    ref = read_frame(reference)
    assert len(ref) == 15+640*350*4 and ref[12] == 32, 'Unsupported DOS capture'
    bmp = Path(bitmap).read_bytes()
    assert bmp[:2] == b'BM', 'Not a BMP'
    offset = struct.unpack_from('<I', bmp, 10)[0]
    width, height = struct.unpack_from('<ii', bmp, 18)
    bits = struct.unpack_from('<H', bmp, 28)[0]
    assert height > 0 and bits in (24, 32), 'Unsupported BMP'
    size = bits//8
    stride = (width*size+3)//4*4
    rw, rh = (height*4//3, height) if width*3 >= height*4 else (width, width*3//4)
    left, top = (width-rw)//2, (height-rh)//2
    differences = 0
    for y in range(height):
        for x in range(width):
            p = offset+(height-y-1)*stride+x*size
            actual = bmp[p:p+3]
            if left <= x < left+rw and top <= y < top+rh:
                # SDL's Windows renderer resolves exact nearest-neighbour ties to
                # the lower texel. Subtract one integer numerator unit only at
                # the sampling boundary; do not assume square EGA pixels.
                sx = ((2*(x-left)+1)*640-1)//(2*rw)
                sy = ((2*(y-top)+1)*350-1)//(2*rh)
                q = 15+(sy*640+sx)*4
                expected = ref[q:q+3]
            else:
                expected = bytes((170,0,0))  # BMP is BGR, original sidebar is RGB 0,0,170.
            differences += actual != expected
    print(f'{bitmap}: viewport {rw}x{rh} at ({left},{top}); {differences} differing pixels')
    assert differences == 0, 'Aspect, centering, padding or sampling mismatch'


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit('Usage: verify_layout.py REFERENCE.raw.gz CAPTURE.bmp [CAPTURE.bmp ...]')
    for bitmap in sys.argv[2:]:
        verify(sys.argv[1], bitmap)
