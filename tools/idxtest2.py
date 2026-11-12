import struct, math, collections
from idxdec2 import decode
d=open('../../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3','rb').read()
blob=d[0xbd370:0xbd370+0xb00]; pos=[struct.unpack_from('>3f',d,0xbde70+12*i) for i in range(1606)]
def score(tris):
    V=len(pos); bad=0; L=[]; ec=collections.Counter()
    for t in tris:
        if any(i<0 or i>=V for i in t) or len(set(t))<3: bad+=1; continue
        for a,b in ((t[0],t[1]),(t[1],t[2]),(t[2],t[0])):
            L.append(math.dist(pos[a],pos[b])); ec[(a,b)]+=1
    opp=sum(1 for (a,b) in ec if (b,a) in ec); L.sort()
    return bad, round(L[len(L)//2],4), opp
refs={
 'nxt+e':lambda e,s:s['nxt']+e, 'nxt-e':lambda e,s:s['nxt']-e,'nxt-1-e':lambda e,s:s['nxt']-1-e,'nxt-1+e':lambda e,s:s['nxt']-1+e,
 'last+e':lambda e,s:s['last']+e,'last-e':lambda e,s:s['last']-e,
 'lastexp+e':lambda e,s:s['lastexp']+e,'lastexp-e':lambda e,s:s['lastexp']-e,
 'mx+e':lambda e,s:s['mx']+e,'mx-e':lambda e,s:s['mx']-e,
}
for n,r in refs.items():
    for w in ('qp','pq'):
        print(n,w,score(decode(blob,2370,r,winding=w)))
