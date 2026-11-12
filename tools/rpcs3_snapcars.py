"""Snapshots of the original game (RPCS3) at the free-run start grid, one per car, for the port's original physics.

usage: python rpcs3_snapcars.py [orig:0,1,2,3,4] [dlc:0,1,2,3] [--drive]
Writes capture/cars/<pack><idx>.snap (guest memory, tools/ps3snap.py format). With --drive, right after the first
snapshot a scripted drive (changing inputs) is recorded to capture/cars/<pack><idx>_drive.bin so the recompiled physics
can be compared frame by frame against the emulator from that snapshot.
Keys go to the RPCS3 window only (rpcs3_ctl refuses when it is not in the foreground).
"""
import os, sys, time, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpcs3_ctl as c
rt = None   # rpcs3_test attaches to the emulator when imported: loaded after launching it

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'capture', 'cars')
PY = sys.executable
DRIVE = '-:0.5,P:2.5,P+A:1.0,P:0.8,P+D:0.8,O:1.0,X+A:0.8,P:1.2,-:0.4'


def jobs(argv):
    out = []
    for a in argv:
        if ':' in a and not a.startswith('--'):
            pack, idxs = a.split(':')
            out += [(pack, int(i)) for i in idxs.split(',') if i != '']
    return out or [('orig', i) for i in range(5)] + [('dlc', i) for i in range(4)]


def snapshot(path):
    cars = rt.find_cars()
    car = cars[-1] if cars else 0x10000
    r = subprocess.run([PY, os.path.join(HERE, 'ps3snap.py'), path, '%x' % car, '0'], capture_output=True, text=True)
    print('   ', r.stdout.strip().replace('\n', ' | '), r.stderr.strip()[-200:])
    return cars


def guest_mb():
    """Committed PS3 guest memory of the running emulator, MB (0 while RPCS3 is still booting / compiling shaders)."""
    try:
        from ps3mem import Proc
        from ps3snap import regions
        p = Proc(); p.base = 0x300000000
        return sum(n for a, n in regions(p, p.base, p.base + (1 << 32))) / 1e6
    except Exception:
        return 0


def wait_title(timeout=600):
    t0 = time.time()
    while time.time() - t0 < timeout:
        mb = guest_mb()
        if mb > 300:
            time.sleep(12)                           # title screen up and idle
            return True
        time.sleep(3)
    return False


def to_free_run(idx, pack):
    """Title screen -> free run on the grid with car <idx> of <pack> (sequence checked step by step on screen)."""
    c.focus()
    c.tap('Return', after=4.0)                 # PRESS START -> autosave notice
    c.tap('X', after=8.0)                      # confirm; trophy data loads, then the PSN notice
    c.tap('X', after=3.0)                      # confirm -> main menu (single player selected)
    c.tap('X', after=3.0)                      # single player
    for k in range(3): c.tap('Right', after=0.9)   # free run
    c.tap('X', after=7.0)                      # car select (it ignores the stick while it animates in)
    if pack == 'dlc':
        c.tap('E', after=2.0)                  # car pack
    for k in range(idx): c.tap('D', hold=0.25, after=2.5)   # left stick: next car (the d-pad does nothing here)
    c.shot(os.path.join(OUT, '%s%d_select.png' % (pack, idx)))
    c.tap('X', after=2.0)                      # car
    c.tap('X', after=20.0)                     # automatic gearbox; the track loads


def main():
    global rt
    os.makedirs(OUT, exist_ok=True)
    from live_car import live_car
    for n, (pack, idx) in enumerate(jobs(sys.argv[1:])):
        name = '%s%d' % (pack, idx)
        for attempt in range(2):
            print('==', name, 'attempt', attempt + 1); sys.stdout.flush()
            c.launch()                               # fresh emulator
            time.sleep(5)
            if not wait_title():
                print('    title screen not reached'); continue
            time.sleep(25)                           # licence screens -> PRESS START
            ok = False
            for k in range(5):
                if c.focus(): ok = True; break
                time.sleep(2)
            try:
                if not ok: raise c.NotFocused('no focus')
                to_free_run(idx, pack)
            except c.NotFocused as ex:
                print('    focus lost:', ex); sys.stdout.flush(); continue
            car = None
            for k in range(30):
                car = live_car()
                if car: break
                time.sleep(3)
            c.shot(os.path.join(OUT, name + '.png'))
            if not car:
                print('    no live car (not on track)'); sys.stdout.flush(); continue
            r = subprocess.run([PY, os.path.join(HERE, 'ps3snap.py'), os.path.join(OUT, name + '.snap'), '%x' % car, '0'], capture_output=True, text=True)
            print('    car %x' % car, r.stdout.strip().replace(chr(10), ' | ')); sys.stdout.flush()
            break
    subprocess.run(['powershell', '-NoProfile', '-Command',
                    "Get-Process rpcs3 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '*RPCS3-portable*' } | Stop-Process -Force"])
    print('DONE')


if __name__ == '__main__':
    main()
