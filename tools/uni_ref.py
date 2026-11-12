"""Reference execution of game PPU code in Unicorn (QEMU PPC64 big-endian) to find recompiler bugs by comparison.
usage: python uni_ref.py <fmt>   (runs the game's snprintf 0x457ac0 like src/tools/recomp_fmt.cpp, logs function entries)"""
import sys, struct, os, re
from unicorn import *
from unicorn.ppc_const import *
RE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 're')
d = open(os.path.join(RE, 'EBOOT.elf'), 'rb').read()
mu = Uc(UC_ARCH_PPC, UC_MODE_PPC64 | UC_MODE_BIG_ENDIAN)
mu.mem_map(0x10000, 0x1200000)                       # code + data + bss
mu.mem_write(0x10000, d[0:0xbce9c8])
mu.mem_write(0xbe0000, d[0xbd0000:0xbd0000 + 0x1442dc])
mu.mem_map(0x60000000, 0x200000)
mu.mem_map(0xd0000000, 0x200000)
starts = set()
for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'recomp', 'full', 'game_table.cpp')):
    m = re.match(r'\s+\{0x([0-9a-f]+), f_', l)
    if m: starts.add(int(m.group(1), 16))
fmt = sys.argv[1].encode() if len(sys.argv) > 1 else b'Data/SumoTool/%s/%s/%s [%d] %x %5.2f'
mu.mem_write(0x60001000, fmt + b'\0')
for a, s in ((0x60001100, b'ps3'), (0x60001200, b'accomplishments'), (0x60001300, b'accomplishments')): mu.mem_write(a, s + b'\0')
sp = 0xD0100000 - 0x100
regs = {1: sp, 2: 0xc6f210, 3: 0x60000000, 4: 0x200, 5: 0x60001000, 6: 0x60001100, 7: 0x60001200, 8: 0x60001300, 9: 42, 10: 0xbeef, 13: 0x60100000 + 0x7030}
for r, v in regs.items(): mu.reg_write(UC_PPC_REG_0 + r, v)
mu.mem_write(sp + 0x30 + 8 * 8, struct.pack('>d', 3.14159))
mu.reg_write(UC_PPC_REG_MSR, mu.reg_read(UC_PPC_REG_MSR) | 0x8000000000000000 | 0x2000 | 0x2000000)   # 64-bit, FP and VMX enabled
MAGIC = 0x60180000
mu.mem_write(MAGIC, b'\x48\x00\x00\x00')            # b . (stop address)
mu.reg_write(UC_PPC_REG_LR, MAGIC)
trace = '--trace' in sys.argv
def hook(uc, addr, size, ud):
    if addr in starts and trace:
        print('[fn] %x r3=%x r4=%x r5=%x r6=%x' % (addr, *(uc.reg_read(UC_PPC_REG_0 + i) & 0xffffffffffffffff for i in (3, 4, 5, 6))))
    if addr == MAGIC: uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook)
mu.emu_start(0x457ac0, 0, count=2000000)
out = bytes(mu.mem_read(0x60000000, 0x200)).split(b'\0')[0]
print('returned', mu.reg_read(UC_PPC_REG_3), 'result:', out)
