"""Port (physlab, flat ground, full throttle) vs original (RPCS3 measurement) acceleration times."""
import re, subprocess, sys
car = sys.argv[1] if len(sys.argv) > 1 else '../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3'
out = subprocess.run([r'C:/Users/User/Desktop/GTi Club/GTiPort/build-test/physlab.exe', car, 'accel'], capture_output=True, text=True).stdout
M = [20, 40, 60, 80, 100, 120, 140, 160, 170]
O = [0.32, 0.65, 1.15, 1.74, 2.37, 3.66, 4.94, 7.31, 8.70]
k = 0; t0 = None
for line in out.splitlines():
    m = re.match(r't=\s*([\d.]+) v=\s*([\d.]+)kmh g=(\d)', line)
    if not m: continue
    t, v = float(m.group(1)), float(m.group(2))
    if t0 is None and v > 0.5: t0 = t
    while k < len(M) and v >= M[k]:
        print('%4d km/h  port %5.2f s   original %5.2f s' % (M[k], t - (t0 or 0), O[k])); k += 1
for kk in range(k, len(M)): print('%4d km/h  port   --     original %5.2f s' % (M[kk], O[kk]))
