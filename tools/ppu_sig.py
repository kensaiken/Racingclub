import struct, sys
from capstone import *
d=open(sys.argv[1],'rb').read()
code=d[0x10000:0xbb3000]  # text region (file off 0 -> va 0x10000); skip header
md=Cs(CS_ARCH_PPC, CS_MODE_64|CS_MODE_BIG_ENDIAN); md.detail=False; md.skipdata=True
base_va=0x10000+0x10000
ins=list(md.disasm(code, base_va))
print('ins', len(ins))
import pickle; pickle.dump([(i.address,i.mnemonic,i.op_str) for i in ins], open('ppu_dis.pkl','wb'))
