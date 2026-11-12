import collections, struct
from ep3 import EP3

e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3")
base = lambda s: s.replace(chr(92), '/').split('/')[-1]
c = collections.Counter(); ex = []
for it in e.items:
    if it.tag != 'EMDH': continue
    m = e.material(it)
    if [t for t, _ in m['slots']][:2] == [2, 2]:
        f = [e.texture(e.items[x])['format'] for _, x in m['slots']]
        c[tuple(hex(v) for v in f)] += 1
        if len(ex) < 6:
            ex.append((m['name'].split('::')[-1], [base(e.name(e.items[x])) for _, x in m['slots']],
                       [hex(v) for v in struct.unpack_from('>4I', e.d, it.off + 0x50)]))
print(c)
for x in ex: print(x)
ent = [i for i in e.items if i.tag == 'EEDH'][0]
mats = {it.index: e.material(it) for it in e.items if it.tag == 'EMDH'}
n = 0
for ms in e.meshes(ent):
    m = mats.get(ms['material'])
    if m and [t for t, _ in m['slots']][:2] == [2, 2] and ms['colors']:
        a = [col[3] for col in ms['colors']]; r = [col[0] for col in ms['colors']]
        print('alpha', min(a), max(a), 'mean', sum(a) // len(a), 'red', min(r), max(r))
        n += 1
        if n > 5: break
