import struct, re, sys
from capstone import *
d=open(sys.argv[1],'rb').read()
phoff=struct.unpack_from('>Q',d,0x20)[0]; phnum=struct.unpack_from('>H',d,0x38)[0]
segs=[]
for i in range(phnum):
    o=phoff+i*0x38
    t,fl,off,va,pa,fs,ms,al=struct.unpack_from('>IIQQQQQQ',d,o)
    segs.append((t,fl,off,va,fs,ms)); 
for s in segs: print([hex(x) for x in s])
