"""Load a recording made by rpcs3_rec.py -> (times[], objs[], frames[t][obj] raw bytes)."""
import struct
def load(path):
    d = open(path, 'rb').read()
    n, N, magic = struct.unpack_from('<III', d, 0)
    objs = struct.unpack_from('<%dI' % n, d, 12)
    o = 12 + 4 * n
    ts, frames = [], []
    step = 8 + n * N
    while o + step <= len(d):
        ts.append(struct.unpack_from('<d', d, o)[0])
        frames.append([d[o + 8 + i * N: o + 8 + (i + 1) * N] for i in range(n)])
        o += step
    return ts, objs, frames, N
def f3(b, off): return struct.unpack_from('>3f', b, off)
