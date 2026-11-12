import struct, sys, collections
from ep3 import EP3

e = EP3(sys.argv[1])
names = collections.Counter()
for it in e.items:
    if it.tag != 'EODH': continue
    nm = e.name(it).split('|')[-1].replace('EM_LOCATOR_', '')
    tag = it.strings[-1] if it.strings else ''
    m = struct.unpack_from('>16f', e.d, it.off + 0x60)
    print('%-40s %-28s pos=(%.1f, %.1f, %.1f) fwd=(%.2f, %.2f, %.2f)' % (nm[:40], tag[:28], m[12], m[13], m[14], m[8], m[9], m[10]))
