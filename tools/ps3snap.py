"""Freeze RPCS3, copy the PS3 guest memory to a file, resume, then record the next frames of one object.

usage: python ps3snap.py out.snap <car_obj_hex> [frames]
Snapshot format: 'PS3S' u32 count, then per region: u32 guest_addr, u32 size, bytes.
After resuming, the car object (0x1000 bytes) is recorded for the following simulation frames into out.snap.next
(rec_read.py format), so the recompiled update can be compared against what the emulator computed.
Only reads emulator memory; suspends/resumes the process briefly.
"""
import sys, os, time, struct, ctypes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ps3mem import Proc, MBI, k32

ntdll = ctypes.WinDLL('ntdll')
PROCESS_SUSPEND_RESUME = 0x0800


def regions(p, lo, hi):
    addr, mbi = lo, MBI()
    while addr < hi and k32.VirtualQueryEx(p.h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        base = mbi.BaseAddress or 0
        size = mbi.RegionSize
        if mbi.State == 0x1000 and not (mbi.Protect & 0x100) and mbi.Protect not in (0, 1):
            s, e = max(base, lo), min(base + size, hi)
            if e > s: yield s, e - s
        addr = base + size


def main():
    out, car = sys.argv[1], int(sys.argv[2], 16)
    nframes = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    p = Proc(); p.base = 0x300000000
    h = k32.OpenProcess(PROCESS_SUSPEND_RESUME, False, p.pid)
    ntdll.NtSuspendProcess(h)
    t0 = time.perf_counter()
    try:
        regs = list(regions(p, p.base, p.base + (1 << 32)))
        with open(out, 'wb') as f:
            f.write(b'PS3S' + struct.pack('<I', len(regs)))
            total = 0
            for a, n in regs:
                data = p.read(a, n) or b'\0' * n
                f.write(struct.pack('<II', a - p.base, len(data))); f.write(data); total += len(data)
        car0 = p.r(car, 0x1000)
    finally:
        ntdll.NtResumeProcess(h)
    print('snapshot %d regions %.1f MB in %.1f s' % (len(regs), total / 1e6, time.perf_counter() - t0))
    # following frames
    N = 0x1000
    with open(out + '.next', 'wb') as f:
        f.write(struct.pack('<III', 1, N, 0x47544931)); f.write(struct.pack('<I', car))
        f.write(struct.pack('<d', 0.0)); f.write(car0)
        last, got, t1 = car0[0x90:0x9c] + car0[0x140:0x150], 0, time.perf_counter()
        while got < nframes and time.perf_counter() - t1 < 5:
            b = p.r(car, N)
            k = b[0x90:0x9c] + b[0x140:0x150]
            if k != last:
                f.write(struct.pack('<d', time.perf_counter() - t1)); f.write(b); last = k; got += 1
    print('next frames', got)


if __name__ == '__main__':
    main()
