import collections, struct, sys
from ep3 import EP3

e = EP3(sys.argv[1])
print(collections.Counter(i.tag for i in e.items))
for it in e.items:
    if it.tag in ('EMTH', 'EMDH', 'EEDH', 'EIDH'):
        continue
    print(it.tag, hex(it.off), hex(it.size), [s[:70] for s in it.strings[:3]])
    if it.tag.startswith('EL') or 'Light' in ''.join(it.strings) or 'Sun' in ''.join(it.strings):
        d = e.d; o = it.off
        for p in range(o + 0x30, o + min(it.size, 0x120), 16):
            ws = struct.unpack_from('>4I', d, p)
            fl = struct.unpack_from('>4f', d, p)
            print('   +%03x' % (p - o), ' '.join('%08x' % w for w in ws), ' '.join('%.3f' % f for f in fl))
