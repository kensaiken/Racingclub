import sys, struct, re, collections
data = open(sys.argv[1],'rb').read()
pat = re.compile(rb'[A-Z]{2}ME')
cnt = collections.Counter(); first = {}
for m in pat.finditer(data):
    o = m.start()
    if o % 4: continue
    t = data[o:o+4].decode()
    cnt[t]+=1
    first.setdefault(t, []).append(o)
for t,c in cnt.most_common():
    print(t[::-1], c, [hex(x) for x in first[t][:6]])
