"""Wait until the player drives (raw throttle > 0.5 on a live car), then record it. usage: rec_wait.py out.bin seconds"""
import sys, time, struct, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpcs3_test as rt
from ps3mem import Proc
out, secs = sys.argv[1], float(sys.argv[2])
car = None
while car is None:
    time.sleep(2)
    try:
        rt.p = Proc(); rt.p.base = 0x300000000
        for o in rt.find_cars():
            if (rt.p.f32(o + 0x410) or [0])[0] > 0.5: car = o; break
    except Exception:
        pass
print('recording car', hex(car)); sys.stdout.flush()
N = 0x1000
with open(out, 'wb') as f:
    f.write(struct.pack('<III', 1, N, 0x47544931)); f.write(struct.pack('<I', car))
    t0 = time.perf_counter(); last = None; n = 0
    while time.perf_counter() - t0 < secs:
        b = rt.p.r(car, N) or b'\0' * N
        k = b[0x90:0x9c] + b[0x410:0x424]
        if k != last:
            f.write(struct.pack('<d', time.perf_counter() - t0)); f.write(b); last = k; n += 1
        time.sleep(0.003)
print('frames', n)
