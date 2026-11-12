from ep3 import EP3

base = lambda s: s.replace(chr(92), '/').split('/')[-1]
e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/cote d'azur.ep3")
for it in e.items:
    if it.tag != 'EEDH': continue
    ms = e.meshes(it)
    info = []
    for m in ms:
        mat = e.material(e.items[m['material']])
        info.append((mat['name'].split('::')[-1], [(hex(t), base(e.name(e.items[x]))) for t, x in mat['slots']], len(m['triangles'])))
    print(e.name(it).split('|')[-1], info)
