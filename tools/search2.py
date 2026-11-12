import struct, itertools, collections
from idxdec import BR
d=open('../../NPEB00060/USRDIR/data/cars/pack_original_0/minicooper_0/minicooper.ep3','rb').read()
blob=d[0xbd370:0xbd370+0xb00]; V=1606
A,B,C,D,E=struct.unpack_from('>HHHBB',blob,0)
mb=[(blob[8+(i>>3)]>>(7-(i&7)))&1 for i in range(C*8)]
br=BR(blob,8+C); codes=[br.bits(2) for _ in range(2370)]; br.align()
raw=[br.bits(D) for _ in range(A)]
TRI=list(itertools.permutations(range(3)))
def run(refk, sgn, k0, perm, order, newpos):
    mi=0;ei=0;nxt=0;out=[];prev=None;last=0;lastexp=0;bad=0
    def get():
        nonlocal mi,ei,nxt,last,lastexp
        m=mb[mi]; mi+=1
        if m==0: v=nxt; nxt+=1
        else:
            e=raw[ei]-B; ei+=1
            base={'last':last,'nxt':nxt,'lastexp':lastexp,'mx':nxt-1}[refk]
            v=base+sgn*e+k0; lastexp=v
        last=v; return v
    for c in codes:
        if c==3 or prev is None:
            tri=[get(),get(),get()]
        else:
            a,b,cc=prev; p,q=[(cc,a),(b,cc),(a,b)][perm[c]]
            if newpos: last=q  # pretend sequence order
            n=get(); base=(q,p,n); tri=[base[i] for i in order]
        if len(set(tri))<3 or min(tri)<0 or max(tri)>=V: bad+=1
        out.append(tri); prev=tri
    return bad
res=[]
for refk in ('last','lastexp','nxt'):
  for sgn in (1,-1):
    for k0 in (-2,-1,0,1,2):
      for perm in itertools.permutations(range(3)):
        for order in TRI:
          res.append((run(refk,sgn,k0,perm,order,False),refk,sgn,k0,perm,order))
res.sort()
for r in res[:12]: print(r)
