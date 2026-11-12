"""List mesh segments with vertices near a world point: probe_at.py x y z [radius]"""
import sys, math, struct
from ep3 import EP3

base = lambda s: s.replace(chr(92), '/').split('/')[-1]
x, y, z = map(float, sys.argv[1:4]); r = float(sys.argv[4]) if len(sys.argv) > 4 else 6
e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3")
ent = [i for i in e.items if i.tag == 'EEDH'][0]
for ms in e.meshes(ent):
    near = sum(1 for p in ms['positions'] if math.dist(p, (x, y, z)) < r)
    if not near: continue
    it = e.items[ms['material']]; m = e.material(it)
    sh = struct.unpack_from('>2I', e.d, it.off + 0x50)
    slots = []
    for ty, t in m['slots']:
        tx = e.texture(e.items[t])
        slots.append('%x:%s(%dx%d f%x%s)' % (ty, base(e.name(e.items[t])), tx['width'], tx['height'], tx['format'], ' CUBE' if tx['cubemap'] else ''))
    print(near, m['name'].split('::')[-1], '%x %x' % sh, slots, [a[3] for a in ms['attrs'] or []])
