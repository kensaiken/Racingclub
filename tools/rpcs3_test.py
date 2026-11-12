"""Scripted driving test on the original game (RPCS3) with simultaneous recording.

usage: python rpcs3_test.py out.bin script
script: comma separated steps, each  KEYS:SECONDS  (KEYS joined with '+', '-' = no key)
  e.g.  "-:1,P:6,O:4,-:1"   = wait 1 s, throttle 6 s, brake 4 s, release 1 s
Keys (profile 'auto'): P throttle (R2), O brake (L2), A/D steer, X cross (handbrake?), C circle, Return start.
Records the player's car object (the one whose raw throttle +0x410 follows our input).
"""
import sys, struct, time, threading
sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from ps3mem import Proc
import rpcs3_ctl as c

N = 0x1000
p = Proc()
p.base = 0x300000000


def find_cars():
    pat = struct.pack('>I', 0x00bfc0e0)
    out = []
    for lo, hi in ((0x00010000, 0x10000000), (0x20000000, 0x40000000)):
        a = lo
        while a < hi:
            d = p.r(a, 0x100000)
            if d:
                i = d.find(pat)
                while i >= 0:
                    if i % 16 == 0 and (p.f32(a + i + 0x38c) or [0])[0] > 300:
                        out.append(a + i)
                    i = d.find(pat, i + 1)
            a += 0x100000
    return out


def run(out, script, car=None):
    cars = (car if isinstance(car, list) else [car]) if car else find_cars()
    stop = threading.Event()
    frames = []

    def rec():
        last = None
        t0 = time.perf_counter()
        while not stop.is_set():
            blobs = [p.r(o, N) or b'\0' * N for o in cars]
            key = b''.join(b[0x90:0x9c] for b in blobs) + b''.join(b[0x410:0x424] for b in blobs)
            if key != last:
                frames.append((time.perf_counter() - t0, blobs))
                last = key
            time.sleep(0.002)

    th = threading.Thread(target=rec, daemon=True)
    c.focus()
    th.start()
    held = set()
    try:
        for step in script.split(','):
            keys, secs = step.rsplit(':', 1)
            want = set() if keys == '-' else set(keys.split('+'))
            for k in held - want:
                c.up(k)
            for k in want - held:
                c.down(k)
            held = want
            time.sleep(float(secs))
    finally:
        for k in held:
            c.up(k)
        time.sleep(0.5)
        stop.set()
        th.join()
    with open(out, 'wb') as f:
        f.write(struct.pack('<III', len(cars), N, 0x47544931))
        f.write(struct.pack('<%dI' % len(cars), *cars))
        for t, blobs in frames:
            f.write(struct.pack('<d', t))
            for b in blobs:
                f.write(b)
    print('cars', [hex(o) for o in cars], 'frames', len(frames))


if __name__ == '__main__':
    objs = [int(a, 16) for a in sys.argv[3].split(',')] if len(sys.argv) > 3 else None
    run(sys.argv[1], sys.argv[2], objs)
