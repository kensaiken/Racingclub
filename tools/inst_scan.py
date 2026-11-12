import collections, sys
from ep3 import EP3

BS = chr(92)

def instances(e):
    out = []
    for it in e.items:
        if it.tag != 'EIDH':
            continue
        s = e.d[it.off + 0xa0:it.off + it.size].split(b'\0')[0].decode('latin1')
        src = s.split('::')[0].replace(BS, '/').split('/')[-1]
        nm = s.split('EM_INSTANCE_EM_ENTITY_')[-1] if 'EM_INSTANCE_EM_ENTITY_' in s else s.split('|')[-1]
        out.append((src, nm, it))
    return out

if __name__ == '__main__':
    e = EP3(sys.argv[1])
    inst = instances(e)
    print(len(inst))
    print(collections.Counter(a for a, b, _ in inst).most_common(80))
    print([(a, b) for a, b, _ in inst[:6]], [(a, b) for a, b, _ in inst[300:306]])
