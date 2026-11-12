import sys, struct, zlib
d = open(sys.argv[1],'rb').read()
hdr_end = struct.unpack_from('>I', d, 8)[0]
cands = set()
for o in range(0, len(d)-4, 4):
    v = struct.unpack_from('>I', d, o)[0]
    if hdr_end <= v < len(d) and v % 16 == 0: cands.add((v,o))
ok = {}
for v,o in sorted(cands):
    if v in ok: ok[v][1].append(o); continue
    try:
        z = zlib.decompressobj(-15); r = z.decompress(d[v:v+4000000])
        if z.eof and len(r) > 16: ok[v] = (len(r), [o], len(d[v:v+4000000])-len(z.unused_data))
    except Exception: pass
for v,(n,refs,cs) in sorted(ok.items()):
    print(hex(v), 'raw', hex(n), 'comp', hex(cs), 'refs', [hex(x) for x in refs[:4]])
print(len(ok), 'streams')
