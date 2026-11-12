import struct, sys, re
d=open(sys.argv[1],'rb').read(); base=int(sys.argv[2],0)
u=lambda o: struct.unpack_from('>I',d,o)[0]
# find fixup table: locate "Split Entity For Export" after base
m=d.find(b'Split Entity For Export', base)
# descriptor right after strings, aligned: search for first u32 == compressed stream abs offset pattern
o=(m+0x20)&~15
while u(o)==0 or u(o)>len(d): o+=16
print('stream desc at +%x:'%(o-base), hex(u(o)), hex(u(o+4)), u(o+8), hex(u(o+12)))
fx={}; p=o+16
while p< len(d):
    a,b,c=u(p),u(p+4),u(p+8)
    if a!=1 or b>0x200000 or c>0x200000: break
    fx[b]=c; p+=12
print('fixups', len(fx))
for k in sorted(fx): print('  +%x -> +%x'%(k,fx[k]))
