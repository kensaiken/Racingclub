"""Export all entities of an .ep3 to Wavefront OBJ + PNG textures (diffuse slot) for inspection.
usage: python export_obj.py <file.ep3> <outdir>
"""
import sys, os, math
from ep3 import EP3
import gcmtex

def matmul(a, b):
    return [sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]

def world_matrices(e):
    ents = {it.index: it for it in e.items if it.tag in ('EEDH',)}
    cache = {}
    def wm(i):
        if i in cache: return cache[i]
        it = e.items[i]; m = list(e.entity_matrix(it))
        p = it.parent
        if p != 0xffffffff and p < len(e.items) and e.items[p].tag == 'EEDH':
            m = matmul(m, wm(p))   # row-vector convention: v * local * parent
        cache[i] = m; return m
    return {i: wm(i) for i in ents}

def main(src, out):
    os.makedirs(out, exist_ok=True)
    e = EP3(src)
    base = os.path.splitext(os.path.basename(src))[0]
    texfiles = {}
    for it in e.items:
        if it.tag == 'EMTH':
            try:
                img = gcmtex.decode(e.texture(it))
                fn = 'tex%03d.png' % it.index
                img.save(os.path.join(out, fn)); texfiles[it.index] = fn
            except Exception as ex:
                print('tex', it.index, e.name(it), 'skip:', ex)
    mats = {}
    with open(os.path.join(out, base + '.mtl'), 'w') as mf:
        for it in e.items:
            if it.tag != 'EMDH': continue
            m = e.material(it); name = 'm%03d' % it.index; mats[it.index] = name
            mf.write('newmtl %s\n# %s slots=%s\nKd 1 1 1\n' % (name, m['name'], [(hex(a), b) for a, b in m['slots']]))
            diff = [t for (ty, t) in m['slots'] if (ty & 0xf) == 2 and (ty >> 12) == 0]
            if diff and diff[0] in texfiles: mf.write('map_Kd %s\n' % texfiles[diff[0]])
    W = world_matrices(e)
    vofs = 1
    with open(os.path.join(out, base + '.obj'), 'w') as of:
        of.write('mtllib %s.mtl\n' % base)
        for it in e.items:
            if it.tag != 'EEDH': continue
            M = W[it.index]
            for k, m in enumerate(e.meshes(it)):
                of.write('o %s_%d\nusemtl %s\n' % (e.name(it).split('|')[-1], k, mats.get(m['material'], 'default')))
                for (x, y, z) in m['positions']:
                    X = x*M[0] + y*M[4] + z*M[8] + M[12]; Y = x*M[1] + y*M[5] + z*M[9] + M[13]; Z = x*M[2] + y*M[6] + z*M[10] + M[14]
                    of.write('v %.5f %.5f %.5f\n' % (X, Y, Z))
                uvs = m['uvs'] or [(0, 0)] * len(m['positions'])
                for (u, v) in uvs: of.write('vt %.5f %.5f\n' % (u, 1 - v))
                nr = m['normals'] or [(0, 1, 0)] * len(m['positions'])
                for (x, y, z) in nr:
                    X = x*M[0] + y*M[4] + z*M[8]; Y = x*M[1] + y*M[5] + z*M[9]; Z = x*M[2] + y*M[6] + z*M[10]
                    of.write('vn %.4f %.4f %.4f\n' % (X, Y, Z))
                for (a, b, c) in m['triangles']:
                    a += vofs; b += vofs; c += vofs
                    of.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a, a, a, b, b, b, c, c, c))
                vofs += len(m['positions'])
    print('exported', out)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
