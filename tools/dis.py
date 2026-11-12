# dis.py ADDR [N] [BEFORE] : print disassembly from re/ppu_dis.pkl
import sys, pickle, bisect
dis = pickle.load(open('re/ppu_dis.pkl', 'rb'))
addrs = [a for a, _, _ in dis]
a = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 40; b = int(sys.argv[3]) if len(sys.argv) > 3 else 0
i = bisect.bisect_left(addrs, a) - b
for a, m, o in dis[i:i + n + b]: print('%08x  %-8s %s' % (a, m, o))
