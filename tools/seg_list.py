import sys, struct, re
def segs(d):
    u = lambda o: struct.unpack_from('>I', d, o)[0]
    out=[]
    for m in re.finditer(re.escape(bytes.fromhex('0000000000000000000000a00000000000000001')), d):
        o = m.start()
        if o % 16: continue
        isz = u(o+0x38); p0 = u(o+0x68); p1=u(o+0x70)
        data = o+0x100
        # vertex decl search
        m2 = re.compile(rb'\x81[\x00-\xff]\x00\x00\x00\x00\x30\x00', re.S).search(d, data+isz)
        if not m2: continue
        nv, ni = struct.unpack_from('>HH', d, m2.start()+8)
        hdr = struct.unpack_from('>HHHBB', d, data)
        out.append(dict(off=o, data=data, isz=isz, psz=(p0,p1), decl=m2.start(), nv=nv, ni=ni, hdr=hdr,
                        posbytes=m2.start()-(data+isz)))
    return out
if __name__=='__main__':
    d=open(sys.argv[1],'rb').read()
    for s in segs(d):
        print(hex(s['off']), 'isz',hex(s['isz']), 'psz',[hex(x) for x in s['psz']], 'nv',s['nv'],'ni',s['ni'],'posv',s['posbytes']/12,'hdr',s['hdr'])
