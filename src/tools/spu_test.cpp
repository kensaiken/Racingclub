// Runs an SPU ELF in the interpreter and reports how it talks to the outside (branches into the SPURS kernel area,
// channels, stop codes). usage: spu_test <spu.elf> [maxSteps]
#include "recomp/spu.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace gti::recomp;
static uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
int main(int argc, char** argv) {
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<uint8_t> d; { uint8_t b[65536]; size_t k; while ((k = std::fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + k); std::fclose(f); }
    static Spu s;
    s.reset();
    std::memset(s.ls, 0, sizeof s.ls);
    static std::vector<uint8_t> mainmem(0x1000000);
    s.mem = mainmem.data();
    const uint32_t entry = be32(&d[0x18]), phoff = be32(&d[0x1c]);
    const int phnum = (d[0x2c] << 8) | d[0x2d];
    for (int i = 0; i < phnum; ++i) {
        const uint8_t* ph = &d[phoff + i * 32];
        if (be32(ph) != 1) continue;
        std::memcpy(s.ls + be32(ph + 8), &d[be32(ph + 4)], be32(ph + 16));
    }
    s.pc = entry;
    s.r[1].sw(0, 0x3FFF0);
    int events = 0;
    s.hookBelow = 0x3000;
    s.onBranch = [&](Spu& sp, uint32_t t) { std::printf("branch to kernel 0x%x  r3=%08x r4=%08x r5=%08x lr=%08x\n", t, sp.r[3].w(0), sp.r[4].w(0), sp.r[5].w(0), sp.r[0].w(0)); sp.pc = sp.r[0].w(0); return ++events < 20; };
    s.onStop = [&](Spu& sp, uint32_t code) { std::printf("stop 0x%x at pc 0x%x\n", code, sp.pc); return false; };
    s.onWriteCh = [&](Spu& sp, uint32_t ch, uint32_t v) { std::printf("wrch %u <- %08x\n", ch, v); return ++events < 20; };
    s.onReadCh = [&](Spu& sp, uint32_t ch, uint32_t& v) { std::printf("rdch %u\n", ch); v = 0; return ++events < 20; };
    s.run(argc > 2 ? std::strtoull(argv[2], nullptr, 0) : 2000000);
    std::printf("stopped at pc 0x%x after %llu steps, bad instruction %08x\n", s.pc, (unsigned long long)s.steps, s.badInstr);
}
