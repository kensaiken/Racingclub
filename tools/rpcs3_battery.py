"""Run a battery of scripted tests on the original game for several cars, restarting free run before each test.

usage: python rpcs3_battery.py <car_index>[,<car_index>...] [pack]
  car_index = number of Right presses on the car select screen (0 = default/first car)
  pack: 'dlc' presses E (R1) once on car select to switch to the car pack
Each test is saved to capture/b_<pack><idx>_<name>.bin and summarised.
"""
import sys, time, os, struct, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpcs3_ctl as c
import rpcs3_test as rt
from rec_read import load, f3

CAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'capture')
TESTS = {
    'accel': '-:0.6,P:3.6,O:2.6,-:0.3',
    'steer60': '-:0.6,P:1.7,P+A:0.9,P:0.6,O:1.5,-:0.3',
    'steer40': '-:0.6,P:1.0,P+D:0.9,P:0.6,O:1.5,-:0.3',
    'hbrake': '-:0.6,P:1.9,X+A:1.0,-:0.8,O:1.0,-:0.3',
    'hb40': '-:0.6,P:1.2,X+A:1.2,-:0.8,O:1.0,-:0.3',
    'hb70': '-:0.6,P:2.3,X+D:0.9,-:0.8,O:1.0,-:0.3',
    'brksteer': '-:0.6,P:2.2,O+A:1.0,-:0.5,O:1.0,-:0.3',
    'liftsteer': '-:0.6,P:2.2,D:1.0,-:0.6,O:1.0,-:0.3',
}


def slow(keys, after=0.9):
    for k in keys:
        c.tap(k, after=after)


def to_free_run(idx, pack):
    """From the title screen ('PRESS START') to driving in free run with the chosen car."""
    c.focus()
    c.tap('Return', after=4.0)
    c.tap('X', after=3.0)                      # single player
    slow(['Right'] * 3, 0.8)                   # free run
    c.tap('X', after=3.5)                      # car select
    if pack == 'dlc':
        c.tap('E', after=2.0)
    slow(['Right'] * idx, 1.0)
    c.tap('X', after=1.5)                      # car
    c.tap('X', after=16.0)                     # automatic gearbox, load


def quit_to_title():
    c.focus()
    c.tap('Return', after=1.5)                 # pause
    slow(['Down'] * 3, 0.5)
    c.tap('X', after=1.0)
    c.tap('Left', after=0.5)
    c.tap('X', after=9.0)


def car_obj():
    """The live player car: stale objects from earlier sessions stay in memory, so pick the one whose raw
    throttle (+0x410) follows a short throttle tap."""
    for attempt in range(8):
        cars = rt.find_cars()
        c.focus()
        c.down('P'); time.sleep(0.25)
        live = [o for o in cars if (rt.p.f32(o + 0x410) or [0])[0] > 0.5]
        c.up('P'); time.sleep(1.2)
        if live:
            return live[0]
        time.sleep(2.0)
    return cars[0] if cars else None


def summary(path):
    ts, objs, fr, N = load(path)
    g = lambda j, o: struct.unpack_from('>f', fr[j][0], o)[0]
    out = []
    j0 = next((j for j in range(len(ts)) if g(j, 0x410) > 0.5), 0)
    for j in range(j0, len(ts), 6):
        v = math.sqrt(sum(x * x for x in f3(fr[j][0], 0x140))) * 3.6
        out.append('%.2f:%.0f/%.0f' % (ts[j] - ts[j0], v, g(j, 0x154) * 57.3))
    return ' '.join(out)


if __name__ == '__main__':
    idxs = [int(x) for x in sys.argv[1].split(',')]
    pack = sys.argv[2] if len(sys.argv) > 2 else 'orig'
    for idx in idxs:
        only = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
        for name, script in ((n, TESTS[n]) for n in only):
            to_free_run(idx, pack)
            car = car_obj()
            mass = rt.p.f32(car + 0x38c)[0] if car else 0
            tq = rt.p.f32(car + 0x380, 4) if car else None
            path = os.path.join(CAP, 'b_%s%d_%s.bin' % (pack, idx, name))
            rt.run(path, script, car)
            print('%s car %d %s mass %.0f params %s' % (pack, idx, name, mass, tq))
            print('   t:km/h/yawrate', summary(path)[:700])
            sys.stdout.flush()
            quit_to_title()
