"""Address of the live player car in a running RPCS3: the car object that is a body of the live physics world."""
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps3mem import Proc
from ps3snap import regions


def live_car(p=None):
    if p is None:
        p = Proc(); p.base = 0x300000000
    wv = struct.pack('>I', 0x00bfbaa0)
    for a, n in regions(p, p.base, p.base + (1 << 32)):
        d = p.read(a, n)
        if not d: continue
        i = d.find(wv)
        while i >= 0:
            if i % 16 == 0 and abs(struct.unpack_from('>f', d, i + 0x9c)[0] - 1 / 60) < 1e-4:
                lst, cnt = struct.unpack_from('>II', d, i + 0xc4)
                if 0 < cnt < 4096:
                    raw = p.read(p.base + lst, 4 * cnt) or b''
                    for k in range(len(raw) // 4 - 1, -1, -1):
                        b = struct.unpack_from('>I', raw, 4 * k)[0]
                        if b and b < 0xf0000000:
                            vt = p.read(p.base + b, 4)
                            if vt and struct.unpack('>I', vt)[0] == 0x00bfc0e0:
                                return b
            i = d.find(wv, i + 4)
    return None


if __name__ == '__main__':
    car = live_car()
    print('%x' % car if car else 'none')
