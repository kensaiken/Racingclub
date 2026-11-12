import glob, struct, os, math, collections
from seg_list import segs
from idxdec import BR
rows=[]
for f in glob.glob(r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/**/*.ep3', recursive=True):
    d=open(f,'rb').read()
    for s in segs(d):
        V=s['posbytes']//12
        if not(s['posbytes']%12==0 and s['nv']-8<V<=s['nv']): continue
        A,B,C,D,E=s['hdr']; T=s['ni']//3
        if A==0 or C!=(V+A+7)//8: continue
        blob=d[s['data']:s['data']+s['isz']+16]
        mb=[(blob[8+(i>>3)]>>(7-(i&7)))&1 for i in range(C*8)]
        try:
            br=BR(blob,8+C); codes=[br.bits(2) for _ in range(T)]; br.align(); ex=[br.bits(D) for _ in range(A)]
        except IndexError: continue
        pos=[struct.unpack_from('>3f',d,s['data']+s['isz']+12*i) for i in range(V)]
        mi=ei=nxt=0; lastexp=None
        for t,c in enumerate(codes):
            n=3 if c==3 or t==0 else 1
            m=mb[mi:mi+n]; vals=[]; nat={}
            for k in range(n):
                if m[k]: nat[k]=nxt; vals.append(('E',ex[ei])); ei+=1
                else: vals.append(('N',nxt)); nxt+=1
            mi+=n
            if n==3 and sum(m)==1:
                k=m.index(1); others=[v for kind,v in vals if kind=='N']
                if max(others)>=V: continue
                cand=sorted(range(min(nxt,V)), key=lambda j: math.dist(pos[j],pos[others[0]])+math.dist(pos[j],pos[others[1]]))
                j=cand[0]; same=[x for x in cand[:4] if pos[x]==pos[j]]
                rows.append(dict(f=os.path.basename(f),raw=vals[k][1],B=B,D=D,nxt=nat[k],k=k,best=same,V=V,d=[nat[k]-1-b for b in same],e=vals[k][1]-B))
print(len(rows))
for r in rows[:40]: print(r)
