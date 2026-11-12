"""Run physlab scenarios and print the metrics measured on the original (RPCS3) next to the port's.

usage: python labsum.py [car.ep3]   (env vars such as LATK / INERTK / BRAKEK pass through to physlab)
"""
import os, re, subprocess, sys

EXE = r'C:/Users/User/Desktop/GTi Club/GTiPort/build-test/physlab.exe'
car = sys.argv[1] if len(sys.argv) > 1 else r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3'
RX = re.compile(r't=\s*([\d.]+) v=\s*([\d.]+)kmh g=(\d) steer=([+-][\d.]+) yaw=\s*([+-][\d.]+) yawRate=\s*([+-][\d.]+)')


def run(sc):
    out = subprocess.run([EXE, car, sc], capture_output=True, text=True).stdout
    return [tuple(float(x) for x in m.groups()) for m in map(RX.match, out.splitlines()) if m]


def steer75():
    rows = run('steer75')
    on = next(i for i, r in enumerate(rows) if r[3] > 0.01)
    t0, y0 = rows[on][0], rows[on][4]
    pick = lambda dt: min(rows[on:], key=lambda r: abs(r[0] - t0 - dt))
    yr = [pick(d)[5] for d in (0.1, 0.3, 0.5, 0.8)]
    turned = max(abs(r[4] - y0) for r in rows[on:on + 12])
    return 'steer75 yawrate +0.1/+0.3/+0.5/+0.8s: %s (orig 21/90/108/100)  heading change in 1.1 s: %.0f deg (orig ~180, spins)' % (
        '/'.join('%.0f' % y for y in yr), turned)


def brake():
    rows = run('brake')
    i0 = next(i for i, r in enumerate(rows) if r[1] > 63)
    i1 = next((i for i in range(i0, len(rows)) if rows[i][1] < 14), None)
    rev = max(r[1] for r in rows[i1:]) if i1 else 0
    return 'brake 63->14 km/h: %s s (orig 1.75)  reverse top: %.0f km/h (orig 48)' % (
        '%.2f' % (rows[i1][0] - rows[i0][0]) if i1 else '--', rev)


print(steer75())
print(brake())
