import math, statistics
from ep3 import EP3

e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3")
ent = [i for i in e.items if i.tag == 'EEDH'][0]
for ms in e.meshes(ent):
    m = e.material(e.items[ms['material']])['name'].split('::')[-1]
    if not m.startswith('road2_DirtRoad') or not ms['colors']: continue
    near = [i for i, p in enumerate(ms['positions']) if math.dist(p, (195, 0.5, 228)) < 15]
    if not near: continue
    cols = [ms['colors'][i] for i in near]
    for ch in range(4):
        v = [c[ch] for c in cols]
        print(m, 'ch', ch, 'min', min(v), 'max', max(v), 'mean', round(statistics.mean(v)))
    print(cols[:8])
    break
