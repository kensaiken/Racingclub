"""Decode PS3 CellGcmTexture data (as stored in .ep3) to RGBA Pillow images (top mip only)."""
import struct, io
from PIL import Image

def _dds(fourcc, w, h, data):
    hdr = struct.pack('<4sIIIIIII44sII4sIIIIIIIIII', b'DDS ', 124, 0x1007 | 0x80000, h, w, len(data), 0, 1,
                      b'\0'*44, 32, 4, fourcc, 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0)
    return hdr + data

def _morton(x, y):
    z = 0
    for i in range(16):
        z |= ((x >> i) & 1) << (2*i) | ((y >> i) & 1) << (2*i + 1)
    return z

def decode(tex):
    fmt = tex['format']; w = tex['width']; h = tex['height']; data = tex['data']
    base = fmt & ~0x60          # strip LN / UN flags
    linear = bool(fmt & 0x20)
    if base in (0x86, 0x87, 0x88):
        bs = 8 if base == 0x86 else 16
        size = max(1, (w+3)//4) * max(1, (h+3)//4) * bs
        cc = {0x86: b'DXT1', 0x87: b'DXT3', 0x88: b'DXT5'}[base]
        return Image.open(io.BytesIO(_dds(cc, w, h, data[:size]))).convert('RGBA')
    if base == 0x85:   # A8R8G8B8
        img = Image.new('RGBA', (w, h)); px = img.load()
        for y in range(h):
            for x in range(w):
                o = (y*w + x)*4 if linear else _morton(x, y)*4
                a, r, g, b = data[o:o+4]
                px[x, y] = (r, g, b, a)
        return img
    if base == 0x81:   # B8
        img = Image.new('L', (w, h)); px = img.load()
        for y in range(h):
            for x in range(w):
                px[x, y] = data[(y*w + x) if linear else _morton(x, y)]
        return img.convert('RGBA')
    raise NotImplementedError('gcm format 0x%02x' % fmt)
