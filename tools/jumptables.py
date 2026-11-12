"""Exact switch-table targets of every in-function indirect jump (bctr) of the EBOOT -> re/jt_entries.txt.
Pattern (GCC PPC64): cmplwi crN, rI, MAX ; ... lwz rT, OFF(r2) ; slwi rX, rI, 2 ; lwzx r0, rX, rT ; add r0, r0, rT ; mtctr r0 ; bctr
Table entries are 32-bit offsets relative to the table start."""
import pickle, struct, re, os
RE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 're')
ins = pickle.load(open(os.path.join(RE, 'ppu_dis.pkl'), 'rb'))
idx = {a: i for i, (a, m, o) in enumerate(ins)}
code = set(a for a, m, o in ins if m != '.byte')
d = open(os.path.join(RE, 'EBOOT.elf'), 'rb').read()
def rd(va):
    if 0xbe0000 <= va < 0xbe0000 + 0x1442dc: o = va - 0xbe0000 + 0xbd0000
    elif 0x10000 <= va < 0x10000 + 0xbce9c8: o = va - 0x10000
    else: return None
    return struct.unpack_from('>I', d, o)[0]
TOCS = (0xc6f210, 0xc7f068, 0xc8f064)
out, nt, fail = set(), 0, 0
mapping = {}
for i, (a, m, o) in enumerate(ins):
    if m != 'bctr': continue
    win = ins[max(0, i - 80):i]
    lim = None; offs = []
    for wa, wm, wo in win:
        mm = re.match(r'cr\d+, r\d+, (0x[0-9a-f]+|\d+)$', wo)
        if wm == 'cmplwi' and mm: lim = int(mm.group(1), 0)
        mm = re.match(r'r\d+, (-?0x[0-9a-f]+|-?\d+)\(r2\)$', wo)
        if wm == 'lwz' and mm: offs.append(int(mm.group(1), 0))
    if not offs or not any(wm == 'lwzx' for wa, wm, wo in ins[max(0, i - 12):i]): continue
    if lim is None: lim = -1   # no bound found: read entries while they are valid
    ok = False
    for off in reversed(offs):
        for toc in TOCS:
            T = rd(toc + off)
            if T is None: continue
            tg = []
            for k in range((lim + 1) if lim >= 0 else 1024):
                e = rd(T + 4 * k)
                if e is None: break
                t = (T + (e if e < 0x80000000 else e - (1 << 32))) & 0xffffffff
                if t not in code or abs(t - a) > 0x40000: break
                tg.append(t)
            if (lim >= 0 and len(tg) == lim + 1) or (lim < 0 and len(tg) >= 2):
                out.update(tg); mapping[a] = tg; ok = True; break
        if ok: break
    nt += 1
    if not ok: fail += 1
open(os.path.join(RE, 'jt_map.txt'), 'w').write('\n'.join('%x: %s' % (b, ' '.join('%x' % t for t in tg)) for b, tg in sorted(mapping.items())))
print('switch tables', nt, 'resolved', nt - fail, 'targets', len(out), 'has 0x97472c', 0x97472c in out)
