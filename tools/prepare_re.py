"""Intermediate files the recompiler needs, made from the decrypted game executable alone.

usage: python tools/prepare_re.py <EBOOT.elf> <re dir>

Writes into <re dir>:
  ppu_dis.pkl      disassembly of the code segment, one (address, mnemonic, operands) per word
  opd_targets.txt  entry points: every function descriptor found in the data, plus tools/data/known_entries.txt
                   (entry points that no descriptor refers to: import stubs, start-up code)
  extra_entries.txt  copy of tools/data/extra_entries.txt (entry points found while running the game)

The disassembly text depends on the capstone version: the recompiler was written against capstone 5.0.x.
"""
import os, pickle, shutil, struct, sys

# tools/dis.py would shadow the standard library's dis module (capstone imports inspect, which imports dis)
sys.path[:] = [p for p in sys.path if os.path.abspath(p or '.') != os.path.dirname(os.path.abspath(__file__))]

TOCS = (0xc6f210, 0xc7f068, 0xc8f064)   # the three TOC bases of NPEB00060 1.02
CODE_END = 0xbb3000                      # entry points lie below this address
DIS_END = 0xbc3000                       # the disassembly stops here (code, then the embedded SPU programs)


def segments(d):
    phoff, = struct.unpack('>Q', d[0x20:0x28])
    phentsize, phnum = struct.unpack('>HH', d[0x36:0x3a])
    out = []
    for i in range(phnum):
        t, fl, off, va, pa, fsz, msz, al = struct.unpack('>IIQQQQQQ', d[phoff + i * phentsize: phoff + (i + 1) * phentsize])
        if t == 1 and fsz: out.append((off, va, fsz))
    return out


def main():
    elf, re_dir = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))
    import capstone
    if not capstone.__version__.startswith('5.0'):
        print('warning: capstone %s; the recompiler expects the operand syntax of 5.0.x' % capstone.__version__)
    d = open(elf, 'rb').read()
    segs = segments(d)
    off, va, fsz = segs[0]
    end = DIS_END
    code = d[off: off + (end - va)]
    code += b'\0' * ((end - va) - len(code))
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_64 | capstone.CS_MODE_BIG_ENDIAN)
    md.skipdata = True
    ins = [(a, m, o) for a, size, m, o in md.disasm_lite(code, va)]
    os.makedirs(re_dir, exist_ok=True)
    pickle.dump(ins, open(os.path.join(re_dir, 'ppu_dis.pkl'), 'wb'), protocol=4)
    found = set()
    for off, va, fsz in segs:
        for o in range(0, fsz - 7, 4):
            a, toc = struct.unpack_from('>II', d, off + o)
            if toc in TOCS and 0x10000 <= a < CODE_END and a % 4 == 0: found.add(a)
    known = set(int(x, 16) for x in open(os.path.join(here, 'data', 'known_entries.txt')).read().split())
    open(os.path.join(re_dir, 'opd_targets.txt'), 'w').write('\n'.join('%x' % a for a in sorted(found | known)) + '\n')
    shutil.copyfile(os.path.join(here, 'data', 'extra_entries.txt'), os.path.join(re_dir, 'extra_entries.txt'))
    print('%d instructions, %d entry points (%d from descriptors)' % (len(ins), len(found | known), len(found)))


if __name__ == '__main__':
    main()
