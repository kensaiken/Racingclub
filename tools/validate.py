import glob, os, collections, traceback
from ep3 import EP3
tot=collections.Counter(); errs=[]
for f in sorted(glob.glob(r'C:/Users/User/Desktop/GTi Club/NPEB00060/USRDIR/data/**/*.ep3', recursive=True)):
    try:
        e=EP3(f)
    except Exception as ex:
        errs.append((os.path.basename(f),'open',repr(ex))); continue
    for it in e.items:
        try:
            if it.tag=='EMTH':
                t=e.texture(it); tot['tex']+=1; tot['texfmt_%02x'%t['format']]+=1
            elif it.tag=='EEDH':
                for m in e.meshes(it):
                    V=len(m['positions']); bad=sum(1 for t in m['triangles'] if max(t)>=V or min(t)<0)
                    deg=sum(1 for t in m['triangles'] if len(set(t))<3)
                    tot['seg']+=1; tot['tris']+=len(m['triangles']); tot['badtri']+=bad; tot['degen']+=deg
                    tot['mat_ok' if m['material'] is not None else 'mat_missing']+=1
                    tot['nrm_ok' if m['normals'] else 'nrm_missing']+=1
                    tot['uv_ok' if m['uvs'] else 'uv_missing']+=1
        except Exception as ex:
            errs.append((os.path.basename(f), it.tag, hex(it.off), repr(ex)[:120]))
print(dict(tot)); print(len(errs),'errors')
for x in errs[:25]: print(x)
