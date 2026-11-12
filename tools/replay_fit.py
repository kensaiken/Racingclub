"""Compare the port with the original on replay windows and optionally fit tuning constants.

usage:
  python replay_fit.py score                 -> error with the current defaults (env vars respected)
  python replay_fit.py fit NAME=v,... rounds -> coordinate search over the listed env constants
Error: RMS over all frames of (forward speed m/s, lateral speed m/s, yaw rate deg/s / 10).
"""
import os, subprocess, sys, math

ROOT = r'C:/Users/User/Desktop/GTi Club/GTiPort'
EXE = ROOT + '/build-test/physlab.exe'
TRACK = r"C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3"
WIN = os.environ.get('REPLAY_WIN')
CAR = r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3'
_d = os.environ.get('REPLAY_DIR', ROOT + '/capture/replay')
WIN, REF, OUT = _d + '/win.txt', _d + '/ref.txt', _d + '/port_fit.txt'


def read(path):
    wins, cur = [], None
    for line in open(path):
        if line.startswith('W'):
            cur = []; wins.append(cur)
        else:
            cur.append(tuple(float(x) for x in line.split()))
    return wins


REFW = read(REF)


def score(env=None):
    e = dict(os.environ); e.update({k: str(v) for k, v in (env or {}).items()})
    args = [EXE, '--replay', CAR, WIN, OUT] + ([TRACK] if os.environ.get('REPLAY_TRACK') == '1' else [])
    subprocess.run(args, env=e, check=True)
    port = read(OUT)
    sf = sl = sy = sv = 0.0; n = 0
    for a, b in zip(port, REFW):
        for p, r in zip(a, b):
            sf += (p[0] - r[0]) ** 2; sl += (p[1] - r[1]) ** 2; sy += ((p[2] - r[2]) * 57.3 / 10) ** 2; n += 1
            if len(p) > 3 and len(r) > 3: sv += (p[3] - r[3]) ** 2
    if os.environ.get('SCORE') == 'median':
        # robust: median over windows of the per-window error (collisions are chaotic, a few blow-ups dominate RMS)
        per = []
        for a, b in zip(port, REFW):
            m = len(b)
            per.append(math.sqrt(sum((p[0] - r[0]) ** 2 + (p[1] - r[1]) ** 2 + ((p[2] - r[2]) * 5.73) ** 2 / 100 for p, r in zip(a, b)) / max(m, 1)))
        per.sort()
        globals()['LAST_VY'] = 0
        med = per[len(per) // 2]
        return med, med, 0.0, 0.0
    rf, rl, ry, rv = math.sqrt(sf / n), math.sqrt(sl / n), math.sqrt(sy / n) * 10, math.sqrt(sv / n)
    globals()['LAST_VY'] = rv
    return rf + rl + ry / 10 + rv, rf, rl, ry


if __name__ == '__main__':
    if sys.argv[1] == 'score':
        t, rf, rl, ry = score()
        print('total %.3f  fwd %.3f m/s  lat %.3f m/s  yaw %.2f deg/s  vy %.3f m/s' % (t, rf, rl, ry, LAST_VY))
    else:
        params = dict(kv.split('=') for kv in sys.argv[2].split(','))
        params = {k: float(v) for k, v in params.items()}
        rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 3
        best = score(params)[0]
        print('start', params, '%.4f' % best); sys.stdout.flush()
        step = {k: 0.25 for k in params}
        for r in range(rounds):
            for k in params:
                for mul in (1 + step[k], 1 / (1 + step[k])):
                    trial = dict(params); trial[k] = params[k] * mul
                    s = score(trial)
                    if s[0] < best - 1e-4:
                        best, params = s[0], trial
                        print('  %s -> %.4g  total %.4f (fwd %.3f lat %.3f yaw %.2f)' % (k, trial[k], *s)); sys.stdout.flush()
                        break
                else:
                    step[k] *= 0.5
        print('best', ','.join('%s=%.4g' % kv for kv in params.items()), '%.4f' % best)
