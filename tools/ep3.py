"""Reference parser for GTI Club (Sumo Digital 'Emmersion' engine) .ep3 packages (PS3).

Format summary (all big-endian):
  File = header region [0, hdr_end) of items, followed by data blocks.
  Item / block header: u32 0, tag (4CC, byte-reversed e.g. 'HTME' = EMTH), u32 size, u32 0.
  Header items reference a data block via (+0x10 u32 relOff, +0x14 tag, +0x18 u32 absPtr).
  Item global index = order in header region (textures, materials, entities, cameras...).
  Data block: +0x38 numFixups, +0x3c fixupTableOff; fixups are (1, ptrOff, targetOff) relative
  to the block base; the stream descriptor (absOff, rawSize, 1, compSize) sits at fixupTableOff-0x10
  and points to a raw-deflate stream with GPU data (texture texels / RSX-only vertex streams).
  Meshes use Sony Edge Geometry segments (positions F32x3 + compressed indexes).
"""
import struct, zlib, re

def u32(d, o): return struct.unpack_from('>I', d, o)[0]
def tag_at(d, o): return d[o+4:o+8][::-1].decode('latin1')

class Block:
    def __init__(self, d, base):
        self.d, self.base = d, base
        self.tag = tag_at(d, base)
        self.size = u32(d, base+8)
        nfx, fxoff = u32(d, base+0x38), u32(d, base+0x3c)
        self.fx = {}
        for i in range(nfx):
            one, p, t = struct.unpack_from('>III', d, base+fxoff+12*i)
            self.fx[p] = t
        self.stream = None
        if 0x48 in self.fx:
            so = base + self.fx[0x48]
            abs_off, raw, nchunks = struct.unpack_from('>III', d, so)
            sizes = struct.unpack_from('>%dI' % nchunks, d, so + 12)
            if abs_off and raw:
                parts = []; p = abs_off
                for cs in sizes:
                    z = zlib.decompressobj(-15)
                    parts.append(z.decompress(d[p:p+cs])); p += cs
                self.stream = b''.join(parts)
                assert len(self.stream) == raw, (len(self.stream), raw)
    def ptr(self, rel):
        return self.fx.get(rel)

class BitReader:
    def __init__(s, b, pos): s.b, s.p = b, pos*8
    def bits(s, n):
        v = 0
        for _ in range(n):
            v = (v << 1) | ((s.b[s.p >> 3] >> (7 - (s.p & 7))) & 1); s.p += 1
        return v
    def align(s): s.p = (s.p + 7) & ~7

def decompress_edge_indexes(blob, num_indexes):
    """Sony Edge Geometry compressed triangle-list indexes (reverse engineered).
    header: u16 numDeltas, u16 deltaBias, u16 seqFlagBytes, u8 deltaBits, u8 pad
    then seqFlagBytes of per-needed-index flags (1 = explicit delta, 0 = next sequential vertex),
    then 2-bit per-triangle codes (3 = new triangle, 0/1/2 = share an edge of previous triangle),
    byte align, then numDeltas values of deltaBits each: idx = v - bias + explicit[i-8].
    """
    A, B, C, D, _ = struct.unpack_from('>HHHBB', blob, 0)
    T = num_indexes // 3
    flags = blob[8:8+C]
    br = BitReader(blob, 8 + C)
    codes = [br.bits(2) for _ in range(T)]
    br.align()
    ex = []
    for i in range(A):
        ex.append(br.bits(D) - B + (ex[i-8] if i >= 8 else 0))
    fi = ei = nxt = 0
    out = []
    prev = None
    def get():
        nonlocal fi, ei, nxt
        f = (flags[fi >> 3] >> (7 - (fi & 7))) & 1; fi += 1
        if f == 0:
            v = nxt; nxt += 1; return v
        v = ex[ei]; ei += 1; return v
    for c in codes:
        if c == 3 or prev is None:
            tri = (get(), get(), get())
        else:
            a, b, cc = prev
            p, q = ((cc, a), (b, cc), (a, b))[c]
            tri = (q, p, get())
        out.append(tri); prev = tri
    return out

def decode_x11y11z10n(w):
    x = w >> 21; y = (w >> 10) & 0x7ff; z = w & 0x3ff
    if x & 0x400: x -= 0x800
    if y & 0x400: y -= 0x800
    if z & 0x200: z -= 0x400
    return (x / 1023.0, y / 1023.0, z / 511.0)

def half(h):
    return struct.unpack('>e', struct.pack('>H', h))[0]

class Item:
    pass

class EP3:
    def __init__(self, path):
        self.path = path
        d = self.d = open(path, 'rb').read()
        assert tag_at(d, 0) == 'EMDF'
        self.hdr_end = u32(d, 8)
        o = 0x100
        while o < self.hdr_end and not (u32(d, o) == 0 and re.fullmatch(rb'[A-Z]{4}', d[o+4:o+8])):
            o += 4
        self.items = []
        while o < self.hdr_end:
            tag = tag_at(d, o); size = u32(d, o+8)
            if not re.fullmatch(r'[A-Z]{4}', tag) or size == 0: break
            it = Item(); it.off = o; it.tag = tag; it.size = size
            it.ref_tag = tag_at(d, o+0x10); it.data = u32(d, o+0x18)
            it.parent = u32(d, o+0x28) if tag_at(d, o+0x20) == 'EMUK' else 0xffffffff
            strs = re.findall(rb'[\x20-\x7e]{3,}', d[o+0x20:o+size])
            it.strings = [s.decode() for s in strs]
            it.index = len(self.items)
            self.items.append(it)
            o = (o + size + 15) & ~15
        self._blocks = {}

    def block(self, abs_off):
        if abs_off not in self._blocks: self._blocks[abs_off] = Block(self.d, abs_off)
        return self._blocks[abs_off]

    def name(self, it):
        cand = [s for s in it.strings if ('/' in s or '\\' in s or s.startswith('Em')) ]
        s = cand[0] if cand else (it.strings[0] if it.strings else '')
        for junk in ('ALIGN', 'ALIG', 'ALI', 'AL', 'pad.'):
            s = s.replace(junk, '') if s.startswith(junk) else s
        return s

    # ---------------- textures ----------------
    def texture(self, it):
        blk = self.block(it.data)
        g = blk.base + blk.fx[0x94]
        fmt, mip, dim, cube, remap, w, h, depth, loc, _, pitch, off = struct.unpack_from('>BBBBIHHHBBII', self.d, g)
        return dict(format=fmt, mipmaps=mip, dimension=dim, cubemap=cube, width=w, height=h, depth=depth,
                    pitch=pitch, remap=remap, data=blk.stream)

    # ---------------- materials ----------------
    def material(self, it):
        d = self.d; o = it.off
        n = u32(d, o + 0x90)
        slots = []
        for i in range(n):
            ty, _, tex = struct.unpack_from('>III', d, o + 0xe0 + 12*i)
            slots.append((ty, tex))
        return dict(name=self.name(it), slots=slots, flags=u32(d, o+0x50))

    # ---------------- entities / meshes ----------------
    def entity_matrix(self, it):
        return struct.unpack_from('>16f', self.d, it.off + 0x60)

    def meshes(self, it):
        """Yield dicts(material, positions, normals, uvs, triangles) for entity item `it`."""
        if it.tag != 'EEDH' or it.data == 0: return []
        G = self.block(it.data)
        d = self.d; out = []
        n = 0; seen = set()
        while n is not None and n not in seen:
            seen.add(n)
            nb = G.base + n
            nseg = u32(d, nb + 0x128)
            segp = G.ptr(n + 0x12c)
            # material leaves
            segmat = {}
            if segp is not None:
                for lo in range(n + 0x130, segp, 16):
                    a = G.base + lo
                    if u32(d, a) or u32(d, a+4) or u32(d, a+0xc) or u32(d, a+0x10): continue
                    v = u32(d, a+8); m = u32(d, a+0x14)
                    if (v & 0xffff) and (v >> 16) < nseg and m < len(self.items) and self.items[m].tag == 'EMDH':
                        for k in range(v >> 16, (v >> 16) + (v & 0xffff)): segmat.setdefault(k, m)
            for s in range(nseg if segp is not None else 0):
                S = segp + s*0x100
                isz = u32(d, G.base+S+0x38); ip = G.ptr(S+0x3c)
                p0s, p0 = u32(d, G.base+S+0x68), G.ptr(S+0x6c)
                p1s, p1 = u32(d, G.base+S+0x70), G.ptr(S+0x74)
                cfgp = G.ptr(S+0xac)
                rdescp = G.ptr(S+0xe4); rsize = u32(d, G.base+S+0xe8); roff = u32(d, G.base+S+0xec)
                cfg = struct.unpack_from('>BBBBBBBBHHI', d, G.base+cfgp)
                in_fmt, nverts_al, nidx = cfg[2], cfg[8], cfg[9]
                posb = d[G.base+p0:G.base+p0+p0s] + (d[G.base+p1:G.base+p1+p1s] if p1 is not None and p1s else b'')
                assert in_fmt == 0, 'unsupported input fmt %d' % in_fmt
                tris = decompress_edge_indexes(d[G.base+ip:G.base+ip+isz], nidx)
                nv = max(max(t) for t in tris) + 1 if tris else 0
                assert nv * 12 <= len(posb)
                pos = [struct.unpack_from('>3f', posb, 12*i) for i in range(nv)]
                # RSX-only stream
                nrm = uv = None; col = []; attrs = []
                if rdescp is not None and G.stream is not None:
                    rd = d[G.base+rdescp:G.base+rdescp+u32(d, G.base+S+0xe0)]
                    nattr, stride = rd[0], rd[1]
                    attrs = [rd[8+8*i:16+8*i] for i in range(nattr)]
                    vb = G.stream[roff:roff+rsize]
                    nrm = []; uv = []; col = []; a17 = []
                    for i in range(nv):
                        base = i*stride
                        for a in attrs:
                            aoff, afmt, acnt, aid = a[0], a[1], a[2], a[3]
                            if aid == 2 and afmt == 6:
                                nrm.append(decode_x11y11z10n(struct.unpack_from('>I', vb, base+aoff)[0]))
                            elif aid == 5 and afmt == 3:
                                h0, h1 = struct.unpack_from('>HH', vb, base+aoff)
                                uv.append((half(h0), half(h1)))
                            elif aid == 5 and afmt == 2:
                                uv.append(struct.unpack_from('>2f', vb, base+aoff))
                            elif aid == 17 and afmt == 2:
                                a17.append(struct.unpack_from('>3f', vb, base+aoff))
                            elif aid in (4, 9) and afmt == 4 and acnt == 4:
                                col.append(tuple(vb[base+aoff:base+aoff+4]))
                    if len(nrm) != nv: nrm = None
                    if len(uv) != nv: uv = None
                out.append(dict(node=n, seg=s, material=segmat.get(s), positions=pos, normals=nrm, uvs=uv,
                                triangles=tris, num_indexes=nidx, a17=(a17 if nrm is not None and len(a17)==nv else None), colors=(col if nrm is not None and len(col)==nv else None),
                                attrs=[tuple(a[:6]) for a in attrs] if rdescp is not None else None))
            n = G.ptr(n + 0xb4)
        return out
