"""Rewrite env-overridable tuning defaults in src/game/vehicle.cpp.  usage: python set_defaults.py NAME=value,..."""
import re, sys
def F(v):
    t = '%.4g' % float(v)
    return (t if ('.' in t or 'e' in t) else t + '.0') + 'f'
p = r'C:/Users/User/Desktop/GTi Club/GTiPort/src/game/vehicle.cpp'
s = open(p, encoding='utf-8').read()
for kv in sys.argv[1].split(','):
    k, v = kv.split('=')
    if k.startswith('TQG'):
        g = int(k[3:])
        m = re.search(r'const float def\[8\] = \{([^}]*)\};', s)
        vals = [x.strip() for x in m.group(1).split(',')]
        vals[g] = F(v)
        s = s[:m.start(1)] + ', '.join(vals) + s[m.end(1):]
        continue
    pat = r'(std::getenv\("%s"\)\)\) : )[-0-9.e]+f;' % k
    s, n = re.subn(pat, lambda m: m.group(1) + F(v) + ';', s)
    assert n == 1, (k, n)
open(p, 'w', encoding='utf-8').write(s)
print('ok')
