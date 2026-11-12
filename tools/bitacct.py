import sys, struct, glob, re
from seg_list import segs
rows=[]
for f in glob.glob(r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/**/*.ep3', recursive=True):
    d=open(f,'rb').read()
    for s in segs(d):
        if s['posbytes']%12 or s['posbytes']//12 > s['nv'] or s['nv']-s['posbytes']//12>=8 and s['posbytes']//12%8!=0: continue
        V=s['posbytes']//12
        if not (s['nv']-8 < V <= s['nv']): continue
        blob=d[s['data']:s['data']+s['isz']]
        n=len(blob.rstrip(b'\x00'))
        A,B,C,D,E=s['hdr']
        rows.append((V,s['ni'],A,B,C,D,E,n,s['isz']))
rows=sorted(set(rows))
print(len(rows))
for r in rows[:60]: 
    V,I,A,B,C,D,E,n,isz=r; T=I//3
    print('V',V,'I',I,'T',T,'A',A,'B',B,'C',C,'D',D,'E',E,'bytes',n,'bits_after_hdr',(n-8)*8, ' rest after 2T+AD', (n-8)*8-2*T-A*D)
