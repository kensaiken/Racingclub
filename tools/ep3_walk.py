import sys, struct, re
d = open(sys.argv[1],'rb').read()
u32 = lambda o: struct.unpack_from('>I', d, o)[0]
hdr_end = u32(8)
print('header region end', hex(hdr_end), 'filesize', hex(len(d)))
o = 0x100
# find first item: scan for pattern 0, tag
while o < hdr_end:
    if u32(o)==0 and re.fullmatch(rb'[A-Z]{4}', d[o+4:o+8]): break
    o += 4
from collections import Counter
c = Counter()
while o < hdr_end:
    tag = d[o+4:o+8][::-1].decode('latin1')
    size = u32(o+8)
    ref_off = u32(o+0x10); ref_tag = d[o+0x14:o+0x18][::-1].decode('latin1'); ptr = u32(o+0x18)
    strs = re.findall(rb'[\x20-\x7e]{4,}', d[o+0x20:o+size])
    strs = [s.decode() for s in strs if not s.startswith(b'pad.') and not re.fullmatch(rb'[A-Z]{4}',s)]
    c[tag]+=1
    if len(sys.argv)>2: print(hex(o), tag, hex(size), ref_tag, hex(ptr), strs[:3])
    o = (o + size + 15) & ~15
    if size == 0: break
print(c)
# data region
o = hdr_end; c2 = Counter()
while o < len(d)-8:
    tag = d[o+4:o+8][::-1].decode('latin1'); size = u32(o+8)
    if not re.fullmatch(r'[A-Z]{4}', tag): print('stop at', hex(o), d[o:o+16].hex()); break
    c2[tag]+=1
    if len(sys.argv)>2: print('  D', hex(o), tag, hex(size))
    o = (o + size + 15) & ~15
print(c2, hex(o))
