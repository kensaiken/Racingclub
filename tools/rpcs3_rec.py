"""Record the raw car objects of a running GTI Club (RPCS3) for later analysis.

usage: python rpcs3_rec.py out.bin seconds obj1 obj2 ...   (object addresses in hex)
Each frame: f64 host time, then for every object N raw bytes (big-endian PS3 memory).
Only reads memory.
"""
import sys, struct, time
sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from ps3mem import Proc

N = 0x1000
out, secs = sys.argv[1], float(sys.argv[2])
objs = [int(a, 16) for a in sys.argv[3:]]
p = Proc()
p.base = 0x300000000
auto = not objs
while auto and not any(p.u32(o + 0x3f0) == 0x3e000000 for o in objs):
    # auto: every live car object (vtable 0x00bfc0e0); the player's one has +0x3f0 == 0.125.
    # keeps scanning until the player's car exists (e.g. while still in the menus)
    objs = []
    time.sleep(2)
    pat = struct.pack('>I', 0x00bfc0e0)
    for lo, hi in ((0x00010000, 0x10000000), (0x20000000, 0x40000000)):
        a = lo
        while a < hi:
            d = p.r(a, 0x100000)
            if d:
                i = d.find(pat)
                while i >= 0:
                    if i % 16 == 0 and p.f32(a + i + 0x38c)[0] > 300:
                        objs.append(a + i)
                    i = d.find(pat, i + 1)
            a += 0x100000
    objs.sort(key=lambda o: p.u32(o + 0x3f0) != 0x3e000000)   # player first
    print('cars', [hex(o) for o in objs], 'player', hex(objs[0]) if objs and p.u32(objs[0] + 0x3f0) == 0x3e000000 else '?')
    sys.stdout.flush()
with open(out, 'wb') as f:
    f.write(struct.pack('<III', len(objs), N, 0x47544931))
    f.write(struct.pack('<%dI' % len(objs), *objs))
    t0 = time.perf_counter()
    last = None
    frames = 0
    while time.perf_counter() - t0 < secs:
        t = time.perf_counter() - t0
        blobs = [p.r(o, N) or b'\0' * N for o in objs]
        key = blobs[0][0x90:0x9c] + blobs[-1][0x90:0x9c]
        if key != last:              # only store when the simulation advanced
            f.write(struct.pack('<d', t))
            for b in blobs:
                f.write(b)
            frames += 1
            last = key
        time.sleep(0.004)
print('frames', frames)
