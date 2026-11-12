"""Port full-throttle acceleration vs speed (physlab 'accel', flat) next to the original's measured medians."""
import os, re, subprocess, sys
EXE = r'C:/Users/User/Desktop/GTi Club/GTiPort/build-test/physlab.exe'
CAR = r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3'
ORIG = {0: 13.4, 10: 14.0, 20: 16.0, 30: 18.0, 40: 17.8, 60: 9.3, 80: 8.8, 90: 8.2, 100: 6.6, 110: 5.9, 120: 4.4, 130: 4.2, 140: 2.6, 150: 2.3, 160: 2.0, 170: 1.75}
def curve(env=None):
    e = dict(os.environ); e.update({k: str(v) for k, v in (env or {}).items()})
    out = subprocess.run([EXE, CAR, 'accel'], capture_output=True, text=True, env=e).stdout
    rows = [(float(m.group(1)), float(m.group(2)) / 3.6) for m in (re.match(r't=\s*([\d.]+) v=\s*([\d.]+)kmh', l) for l in out.splitlines()) if m]
    bins = {}
    for (t0, v0), (t1, v1) in zip(rows, rows[1:]):
        k = int(v0 * 3.6 // 10) * 10
        if k in ORIG: bins.setdefault(k, []).append((v1 - v0) / (t1 - t0))
    return {k: sorted(v)[len(v) // 2] for k, v in bins.items()}
def err(env=None):
    c = curve(env)
    return sum((c.get(k, 0) - a) ** 2 for k, a in ORIG.items()) ** 0.5, c
if __name__ == '__main__':
    e, c = err()
    print('rms %.2f' % e, ' '.join('%d:%.1f/%.1f' % (k, c.get(k, 0), ORIG[k]) for k in sorted(ORIG)))
