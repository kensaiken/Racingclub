"""Call graph of the decrypted EBOOT (PPU) from re/ppu_dis.pkl.

usage: python ppu_callgraph.py <func_hex> [<func_hex> ...]   -> reachable functions, sizes, indirect calls
Function starts = 'stdu r1,-N(r1)' prologues plus every direct 'bl' target.
"""
import pickle, bisect, sys, os, collections

RE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 're')
ins = pickle.load(open(os.path.join(RE, 'ppu_dis.pkl'), 'rb'))
addrs = [a for a, m, o in ins]
idx = {a: i for i, a in enumerate(addrs)}
starts = set(a for a, m, o in ins if m in ('stwu', 'stdu') and o.startswith('r1, -'))
for a, m, o in ins:
    if m == 'bl':
        try: starts.add(int(o, 16))
        except ValueError: pass
starts = sorted(starts)


def func_range(fa):
    k = bisect.bisect_right(starts, fa)
    end = starts[k] if k < len(starts) else fa + 4
    return fa, end


def body(fa):
    a, e = func_range(fa)
    i = idx.get(a)
    out = []
    while i is not None and i < len(ins) and ins[i][0] < e:
        out.append(ins[i]); i += 1
    return out


def callees(fa):
    direct, indirect = set(), 0
    for a, m, o in body(fa):
        if m == 'bl':
            try: direct.add(int(o, 16))
            except ValueError: pass
        elif m in ('bctrl', 'bcctrl'):
            indirect += 1
    return direct, indirect


def reach(roots, limit=4000):
    seen, q, ind = set(), list(roots), collections.Counter()
    while q and len(seen) < limit:
        f = q.pop()
        if f in seen: continue
        seen.add(f)
        d, n = callees(f)
        if n: ind[f] = n
        q.extend(d - seen)
    return seen, ind


if __name__ == '__main__':
    roots = [int(x, 16) for x in sys.argv[1:]]
    seen, ind = reach(roots)
    total = sum((func_range(f)[1] - f) // 4 for f in seen)
    print('functions', len(seen), 'instructions', total, 'with indirect calls', len(ind))
    for f in sorted(seen):
        print('  %s %5d instr %s' % (hex(f), (func_range(f)[1] - f) // 4, ('indirect x%d' % ind[f]) if f in ind else ''))
