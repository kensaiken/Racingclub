import glob, os, re, collections, sys
from ep3 import EP3
from inst_scan import instances

DATA = '../../NPEB00060/USRDIR/data'
lvl = EP3(DATA + "/levels/cote d'azur/afterburn.ep3")
inst = instances(lvl)

index = collections.defaultdict(list)   # lower name -> [(file, entity name)]
for f in glob.glob(DATA + '/tracksideobjects/**/*.ep3', recursive=True):
    e = EP3(f)
    stem = os.path.splitext(os.path.basename(f))[0].lower()
    index[stem].append((f, '<file>'))
    for it in e.items:
        if it.tag != 'EEDH':
            continue
        nm = e.name(it).split('|')[-1].replace('EM_ENTITY_', '').replace('EmEntity_Uid_', '')
        index[nm.lower()].append((os.path.basename(f), nm))

def base(n):
    return re.sub(r'_\d+$', '', n)

hit = miss = 0
missed = collections.Counter()
for src, nm, it in inst:
    b = base(nm).lower()
    if b in index or nm.lower() in index:
        hit += 1
    else:
        miss += 1; missed[(src, base(nm))] += 1
print('hit', hit, 'miss', miss)
print(missed.most_common(40))
