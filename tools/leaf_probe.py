import struct, math
from ep3 import EP3

e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3")
ent = [i for i in e.items if i.tag == 'EEDH'][0]
G = e.block(ent.data)
for ms in e.meshes(ent):
    m = e.material(e.items[ms['material']])
    if not m['name'].split('::')[-1].startswith('leaves'): continue
    if not any(math.dist(p, (42, 7, 82)) < 8 for p in ms['positions']): continue
    print('attrs', ms['attrs'], 'nverts', len(ms['positions']), 'tris', ms['triangles'][:6])
    print('pos', [tuple(round(c, 2) for c in p) for p in ms['positions'][:8]])
    print('uv', [tuple(round(c, 2) for c in p) for p in (ms['uvs'] or [])[:8]])
    print('a17', [tuple(round(c, 2) for c in p) for p in (ms['a17'] or [])[:8]])
    break
