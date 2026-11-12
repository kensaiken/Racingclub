import struct
from idxdec import BR
def decode(blob, ntris, ref, edges={0:2,1:1,2:0}, winding='qp'):
    A,B,C,D,E = struct.unpack_from('>HHHBB', blob, 0)
    mask = blob[8:8+C]
    br = BR(blob, 8+C)
    codes=[br.bits(2) for _ in range(ntris)]; br.align()
    expl=[br.bits(D)-B for _ in range(A)]
    mbits=[(mask[i>>3]>>(7-(i&7)))&1 for i in range(C*8)]
    st=dict(nxt=0, last=0, lastexp=0, mx=-1, prev=None)
    it=iter(mbits); ei=iter(expl)
    def get():
        if next(it)==0:
            v=st['nxt']; st['nxt']+=1
        else:
            v=ref(next(ei), st)
            st['lastexp']=v
        st['last']=v; st['mx']=max(st['mx'],v)
        return v
    out=[]; prev=None
    for c in codes:
        if c==3 or prev is None:
            tri=(get(),get(),get())
        else:
            a,b,cc=prev
            p,q={0:(a,b),1:(b,cc),2:(cc,a)}[edges[c]]
            tri=(q,p,get()) if winding=='qp' else (p,q,get())
        st['prev']=tri; out.append(tri); prev=tri
    return out
