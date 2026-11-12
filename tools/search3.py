import struct, itertools, collections, math
from idxdec import BR
d=open('../../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3','rb').read()
blob=d[0xbd370:0xbd370+0xb00]; V=1606; T=2370
pos=[struct.unpack_from('>3f',d,0xbde70+12*i) for i in range(V)]
A,B,C,D,E=struct.unpack_from('>HHHBB',blob,0)
mb=[(blob[8+(i>>3)]>>(7-(i&7)))&1 for i in range(C*8)]
br=BR(blob,8+C); codes=[br.bits(2) for _ in range(T)]; br.align()
raw=[br.bits(D)-B for _ in range(A)]
n3=codes.count(3)
def score(tris):
    bad=0; ec=collections.Counter(); L=[]
    for t in tris:
        if any(i<0 or i>=V for i in t) or len(set(t))<3: bad+=1; continue
        for a,b in ((t[0],t[1]),(t[1],t[2]),(t[2],t[0])): ec[(a,b)]+=1; L.append(math.dist(pos[a],pos[b]))
    L.sort(); return bad, sum(1 for (a,b) in ec if (b,a) in ec), round(L[len(L)//2],4)
def run(layout, f, extras_first_in_tri):
    # layout: 'split_main_first' => slots [0..T) one per triangle (the 'third'), then extras 2 per code3 triangle
    ext_i=0; main_i=0
    seq_main=mb[:T] if layout=='main_first' else mb[2*n3:]
    seq_ext=mb[T:T+2*n3] if layout=='main_first' else mb[:2*n3]
    ei=0; nxt=0; out=[]; prev=None; mi_e=0; mi_m=0
    def take(bit):
        nonlocal ei,nxt
        if bit==0: v=nxt; nxt+=1; return v
        e=raw[ei]; ei+=1; return f(e,nxt)
    for t,c in enumerate(codes):
        if c==3 or prev is None:
            b0,b1=seq_ext[mi_e],seq_ext[mi_e+1]; mi_e+=2; bm=seq_main[mi_m]; mi_m+=1
            if extras_first_in_tri: x=take(b0); y=take(b1); z=take(bm)
            else: z=take(bm); x=take(b0); y=take(b1)
            tri=(x,y,z)
        else:
            a,b,cc=prev; p,q=[(cc,a),(b,cc),(a,b)][c]; bm=seq_main[mi_m]; mi_m+=1
            tri=(q,p,take(bm))
        out.append(tri); prev=tri
    return out
F={'n-e':lambda e,n:n-e,'n-1-e':lambda e,n:n-1-e,'n+e':lambda e,n:n+e}
for lay in ('main_first','ext_first'):
    for fn,f in F.items():
        for ef in (True,False):
            print(lay,fn,ef,score(run(lay,f,ef)))
