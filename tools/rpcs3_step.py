"""Manual stepping of the original game in RPCS3: launch | keys K1,K2,... [wait] | shot out.png
usage: python rpcs3_step.py launch            -> starts RPCS3 and waits for the title (guest memory loaded)
       python rpcs3_step.py keys X,Right 1.0  -> taps the keys (after seconds each), then screenshot to capture/step.png
"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rpcs3_ctl as c
OUTP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'capture', 'step.png')
if sys.argv[1] == 'launch':
    from rpcs3_snapcars import wait_title
    c.launch(); time.sleep(5)
    print('title' if wait_title() else 'timeout')
elif sys.argv[1] == 'keys':
    after = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    for k in range(5):
        if c.focus(): break
        time.sleep(1)
    for k in sys.argv[2].split(','):
        if k: c.tap(k, after=after)
    time.sleep(0.5)
c.shot(OUTP)
print('shot')
