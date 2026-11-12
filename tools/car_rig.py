import struct, sys
from ep3 import EP3

e = EP3(sys.argv[1])
for it in e.items:
    if it.tag == 'EODH':
        nm = e.name(it).split('|')[-1]
        m = struct.unpack_from('>16f', e.d, it.off + 0x60)
        print('LOC %-40s %-20s (%.3f, %.3f, %.3f)' % (nm[:40], (it.strings[-1] or '')[:20], m[12], m[13], m[14]))
    if it.tag == 'EEDH':
        nm = e.name(it).split('|')[-1]
        m = struct.unpack_from('>16f', e.d, it.off + 0x60)
        ms = e.meshes(it)
        ys = [p[1] for s in ms for p in s['positions']]; zs = [p[2] for s in ms for p in s['positions']]
        ext = ((max(ys) - min(ys)) if ys else 0, (max(zs) - min(zs)) if zs else 0)
        print('ENT %-40s parent=%-5s local=(%.3f, %.3f, %.3f) meshext_y/z=(%.3f, %.3f)' % (nm[:40], it.parent, m[12], m[13], m[14], ext[0], ext[1]))
