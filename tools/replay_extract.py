"""Cut recordings of the original (RPCS3 captures) into 1-second replay windows for physlab --replay.

usage: python replay_extract.py out_windows.txt out_reference.txt rec1.bin [rec2.bin ...]
A window is kept when the car stays roughly level and flat (no jumps/slopes) and no collision-like jolt occurs.
Reference file: per window "W", then per frame "<vfwd> <vlat> <yawrate>" of the original (same frames as replay).
"""
import sys, struct, math, os
TRACK = os.environ.get('TRACK') == '1'   # real-track replay: keep slopes, jumps, collisions
sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from rec_read import load

DT = 1.0 / 60.0
WIN = 60          # frames per window (1 s)
STEP = 30         # new window every 0.5 s


def f(b, o): return struct.unpack_from('>f', b, o)[0]
def v3(b, o): return struct.unpack_from('>3f', b, o)


def frames_60hz(ts, blobs):
    """Resample to a 60 Hz grid (nearest recorded simulation frame)."""
    out, j = [], 0
    t = ts[0]
    while t <= ts[-1]:
        while j + 1 < len(ts) and abs(ts[j + 1] - t) <= abs(ts[j] - t):
            j += 1
        out.append(blobs[j])
        t += DT
    return out


def state(b):
    X, Y, Z = v3(b, 0x20), v3(b, 0x30), v3(b, 0x40)       # car axes: X = left, Y = up, Z = forward
    vel, ang = v3(b, 0x140), v3(b, 0x150)
    d = lambda a, c: a[0] * c[0] + a[1] * c[1] + a[2] * c[2]
    return d(vel, Z), d(vel, X), d(ang, Y), Y[1], v3(b, 0x90)[1], vel[1]


def gear_for(vf):
    # Mini: 1st up to ~59 km/h, 2nd to ~107, 3rd to ~131 (shift at ~6730 rpm, 382 rpm per m/s in 1st)
    kmh = abs(vf) * 3.6
    if vf < -0.5: return 0
    return 1 if kmh < 57 else 2 if kmh < 104 else 3 if kmh < 128 else 4


def main():
    wout, rout = open(sys.argv[1], 'w'), open(sys.argv[2], 'w')
    kept = 0
    for path in sys.argv[3:]:
        ts, objs, fr, N = load(path)
        blobs = frames_60hz(ts, [x[0] for x in fr])
        S = [state(b) for b in blobs]
        for s in range(0, len(blobs) - WIN - 1, STEP):
            seg = S[s:s + WIN + 1]
            if not TRACK and min(x[3] for x in seg) < 0.97: continue        # tilted (kerb, jump)
            if not TRACK and max(x[4] for x in seg) - min(x[4] for x in seg) > 0.8: continue  # slope / airborne
            if abs(seg[0][0]) < 1.0: continue                              # standing still
            jolt = max(abs(seg[i + 1][0] - seg[i][0]) / DT for i in range(WIN))
            if jolt > 30 and not TRACK: continue                           # collision
            if os.environ.get('ONLYHIT') == '1' and jolt <= 30: continue
            b0 = blobs[s]
            steerN = f(b0, 0x404)
            pose = v3(b0, 0x50) + v3(b0, 0x20) + v3(b0, 0x30) + v3(b0, 0x40) + (seg[0][5],)
            wout.write('W %d %.4f %.4f %.4f %d %.4f %.4f %.4f ' % (WIN, seg[0][0], seg[0][1], seg[0][2], gear_for(seg[0][0]),
                                                                 steerN, f(b0, 0x3f4), f(b0, 0x3f8)) + ' '.join('%.5f' % x for x in pose) + '\n')
            rout.write('W\n')
            for i in range(WIN):
                b = blobs[s + i]
                wout.write('%.3f %.3f %d %.3f\n' % (f(b, 0x410), f(b, 0x418), 1 if f(b, 0x41c) > 0.5 else 0, f(b, 0x420)))
                st = S[s + i + 1]
                rout.write('%.4f %.4f %.4f %.4f\n' % (st[0], st[1], st[2], st[5]))
            kept += 1
    print('windows', kept)


main()
