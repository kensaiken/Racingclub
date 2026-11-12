import struct
class BR:
    def __init__(s, b, pos=0): s.b=b; s.p=pos*8
    def bit(s):
        v=(s.b[s.p>>3]>>(7-(s.p&7)))&1; s.p+=1; return v
    def bits(s,n):
        v=0
        for _ in range(n): v=(v<<1)|s.bit()
        return v
    def align(s): s.p=(s.p+7)&~7

def decode(blob, ntris, variant):
    A,B,C,D,E = struct.unpack_from('>HHHBB', blob, 0)
    mask = blob[8:8+C]
    br = BR(blob, 8+C)
    codes=[br.bits(2) for _ in range(ntris)]
    if variant.get('align_codes', True): br.align()
    expl=[br.bits(D) for _ in range(A)]
    mbits=[(mask[i>>3]>>(7-(i&7)))&1 for i in range(C*8)]
    nxt=0; ei=0; mi=0; out=[]; prev=None
    def get():
        nonlocal nxt, ei, mi
        m=mbits[mi]; mi+=1
        if m==variant.get('newbit',0):
            v=nxt; nxt+=1; return v
        e=expl[ei]; ei+=1
        return variant['expl'](e, nxt, B)
    for t,c in enumerate(codes):
        if c==3 or prev is None:
            tri=(get(),get(),get())
        else:
            a,b,cc=prev
            e=variant['edges'][c]
            p,q={0:(a,b),1:(b,cc),2:(cc,a)}[e]
            tri=(q,p,get())
        out.append(tri); prev=tri
    return out, codes, (A,B,C,D,E)
