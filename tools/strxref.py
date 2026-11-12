# strxref.py EBOOT.elf "string" : find VA of string, TOC slots pointing to it, and code loading those slots
import struct, sys, pickle
d = open(sys.argv[1], 'rb').read()
phoff = struct.unpack_from('>Q', d, 0x20)[0]; phnum = struct.unpack_from('>H', d, 0x38)[0]
segs = []
for i in range(phnum):
    t, fl, off, va, pa, fs, ms, al = struct.unpack_from('>IIQQQQQQ', d, phoff + i * 0x38)
    if t == 1: segs.append((off, va, fs))
def o2v(o):
    for off, va, fs in segs:
        if off <= o < off + fs: return va + o - off
def v2o(v):
    for off, va, fs in segs:
        if va <= v < va + fs: return off + v - va
TOCS = [0xc6f210, 0xc7f068, 0xc8f064]
dis = pickle.load(open('re/ppu_dis.pkl', 'rb'))
idx = {}
for a, m, o in dis:
    if m in ('lwz', 'ld', 'addi'):
        parts = o.split(', ')
        if len(parts) == 2 and parts[1].endswith('(r2)'):
            off = int(parts[1][:-4], 0)
            idx.setdefault(off, []).append((a, m, o))
for s in sys.argv[2:]:
    b = s.encode() + b'\0'
    p = 0
    while True:
        p = d.find(b, p)
        if p < 0: break
        va = o2v(p); p += 1
        if va is None: continue
        print('string', repr(s), 'va', hex(va))
        pat = struct.pack('>I', va)
        q = 0
        while True:
            q = d.find(pat, q)
            if q < 0: break
            slot = o2v(q); q += 4
            if slot is None: continue
            for t in TOCS:
                off = slot - t
                if -0x8000 <= off < 0x8000:
                    for a, m, o in idx.get(off, []):
                        print('   slot', hex(slot), 'toc', hex(t), 'code', hex(a), m, o)
