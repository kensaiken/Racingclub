import struct, math, itertools, collections
from idxdec import decode
d=open('../../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3','rb').read()
def load(seg, V, I, isz):
    blob=d[seg+0x100:seg+0x100+isz]
    pos=[struct.unpack_from('>3f',d,seg+0x100+isz+12*i) for i in range(V)]
    return blob,pos
def score(tris,pos):
    V=len(pos); bad=0; L=[]; ec=collections.Counter()
    for t in tris:
        if any(i<0 or i>=V for i in t) or len(set(t))<3: bad+=1; continue
        for a,b in ((t[0],t[1]),(t[1],t[2]),(t[2],t[0])):
            L.append(math.dist(pos[a],pos[b])); ec[(a,b)]+=1
    dup=sum(1 for k,v in ec.items() if v>1)
    opp=sum(1 for (a,b) in ec if (b,a) in ec)
    L.sort()
    return bad, round(L[len(L)//2],4) if L else None, round(sum(L)/max(1,len(L)),4), dup, opp
cases=[(0xbd270,1606,7110,0xb00),(0xcb7f0,0,0,0)]
blob,pos=load(0xbd270,1606,7110,0xb00)
expls={'abs':lambda e,n,B:e,'n-1-e':lambda e,n,B:n-1-e,'n-e':lambda e,n,B:n-e,'n-1-e-B':lambda e,n,B:n-1-e-B,'n-e+B':lambda e,n,B:n-e+B,'n-1-e+B':lambda e,n,B:n-1-e+B}
for nb in (0,1):
  for al in (True,False):
    for en,ef in expls.items():
        v=dict(newbit=nb,align_codes=al,expl=ef,edges={0:2,1:1,2:0})
        try:
            tris,codes,h=decode(blob,7110//3,v)
            print(nb,al,en,score(tris,pos))
        except Exception as ex: print(nb,al,en,'ERR',ex)
print('----')
for k in range(-3,4):
    for sign in (1,-1):
        v=dict(newbit=0,align_codes=True,expl=(lambda e,n,B,k=k,s=sign: n+s*(e-B)+k),edges={0:2,1:1,2:0})
        tris,codes,h=decode(blob,7110//3,v); print('n',sign,k,score(tris,pos))
