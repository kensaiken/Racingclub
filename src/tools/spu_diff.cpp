// Differential test of the SPU static recompiler: every instruction of every recompiled program is run once in
// the interpreter (spu.cpp) and once through the generated C++ (slow variant, single-step) from the same random
// state; registers, local store and next pc must match bit for bit.
// usage: spu_diff <EBOOT.elf> [trials per instruction = 6]
#include "recomp/spu.h"
#include "recomp/spu_rt.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
using namespace gti::recomp;
using namespace gti::recomp::spurt;

static uint64_t g_seed = 0x9E3779B97F4A7C15ull;
static uint32_t rnd() { g_seed ^= g_seed << 13; g_seed ^= g_seed >> 7; g_seed ^= g_seed << 17; return uint32_t(g_seed >> 16); }
static uint64_t be64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; ++i) v = v << 8 | p[i]; return v; }
static uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

// two NaNs with different payloads count as equal: which operand's payload survives depends on how the host
// compiler orders the operands, and the SPU has no NaNs at all
static bool isNan(uint32_t u) { return (u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu); }

static uint32_t randomWord(int kind) {
    switch (kind) {
        case 0: return rnd();
        case 1: return rnd() & 0x3f;                                   // shift counts, small indices
        case 2: { float x = (int32_t(rnd()) / 65536.0f) * ((rnd() & 3) ? 1.0f : 1e-3f); uint32_t u; std::memcpy(&u, &x, 4); return u; }
        case 3: return (rnd() & 1) ? 0u : ~0u;                          // masks
        case 4: return rnd() & 0x3fff0;                                // local-store addresses
        default: { const uint32_t b = rnd(); return (b & 0xff) * 0x01010101u ^ (rnd() & 0x1f1f1f1f); }   // shufb controls
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: spu_diff <EBOOT.elf> [trials]\n"); return 1; }
    const int trials = argc > 2 ? std::atoi(argv[2]) : 6;
    std::vector<uint8_t> elf;
    {
        FILE* f = std::fopen(argv[1], "rb");
        if (!f) { std::printf("cannot open %s\n", argv[1]); return 1; }
        uint8_t buf[65536]; size_t k;
        while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) elf.insert(elf.end(), buf, buf + k);
        std::fclose(f);
    }
    auto readGuest = [&](uint32_t va, uint8_t* dst, uint32_t n) {
        const uint64_t phoff = be64(&elf[0x20]);
        const int phentsize = elf[0x36] << 8 | elf[0x37], phnum = elf[0x38] << 8 | elf[0x39];
        for (int i = 0; i < phnum; ++i) {
            const uint8_t* p = &elf[phoff + uint64_t(i) * phentsize];
            if (be32(p) != 1) continue;
            const uint64_t off = be64(p + 8), sva = be64(p + 16), fs = be64(p + 32);
            if (va >= sva && va + n <= sva + fs) { std::memcpy(dst, &elf[off + va - sva], n); return true; }
        }
        return false;
    };
    static Spu A, C;                       // interpreter, compiled
    static uint8_t pristine[Spu::LS_SIZE];
    static std::vector<uint8_t> mem(0x10000);
    int totalBad = 0;
    for (int ci = 0; const Compiled* cp = compiled_at(ci); ++ci) {
        for (uint32_t i = 0; i < Spu::LS_SIZE; i += 4) { const uint32_t v = rnd(); std::memcpy(pristine + i, &v, 4); }
        if (!readGuest(cp->eaBin, pristine + cp->lsBase, cp->size)) { std::printf("program %x not in the ELF\n", cp->eaBin); return 1; }
        std::memcpy(A.ls, pristine, sizeof pristine);
        std::memcpy(C.ls, pristine, sizeof pristine);
        A.mem = C.mem = mem.data();
        uint64_t tested = 0, deferred = 0, bad = 0;
        std::map<uint32_t, int> badByOp;
        for (uint32_t pc = cp->lsBase; pc < cp->lsBase + cp->size; pc += 4) {
            const uint32_t word = be32(pristine + pc);
            if ((word >> 21) == 0x00D || (word >> 21) == 0x00F || (word >> 21) == 0x10D) continue;   // channels: not pure
            for (int t = 0; t < trials; ++t) {
                A.reset();
                alignas(16) Q r[128];
                for (int i = 0; i < 128; ++i) {
                    const int kind = (t == 0) ? 0 : int(rnd() % 6);
                    for (int k = 0; k < 4; ++k) { const uint32_t v = randomWord(kind); r[i].w[k] = v; A.r[i].sw(k, v); }
                }
                const uint32_t res = cp->slow[(pc - cp->lsBase) >> 10](r, C, pc, true);
                if ((res & 0x80000000u) && (res & 0x7fffffffu) == pc) { ++deferred; break; }   // left to the interpreter
                A.pc = pc;
                A.badInstr = 0;
                A.step();
                ++tested;
                const char* what = nullptr;
                int reg = -1;
                if (A.badInstr) what = "interpreter does not decode it";
                else if (A.pc != (res & 0x3ffffu)) what = "next pc";
                else {
                    for (int i = 0; i < 128 && !what; ++i)
                        for (int k = 0; k < 4; ++k) if (A.r[i].w(k) != r[i].w[k] && !(isNan(A.r[i].w(k)) && isNan(r[i].w[k]))) { what = "register"; reg = i; break; }
                    if (!what && std::memcmp(A.ls, C.ls, sizeof pristine)) what = "local store";
                }
                if (what) {
                    ++bad;
                    ++badByOp[word >> 21];
                    if (bad <= 12) {
                        std::printf("  MISMATCH program %x pc %05x word %08x (op11 %03x): %s", cp->eaBin, pc, word, word >> 21, what);
                        if (reg >= 0) std::printf(" r%d interp %08x %08x %08x %08x  compiled %08x %08x %08x %08x", reg, A.r[reg].w(0), A.r[reg].w(1),
                                                  A.r[reg].w(2), A.r[reg].w(3), r[reg].w[0], r[reg].w[1], r[reg].w[2], r[reg].w[3]);
                        else if (!std::strcmp(what, "next pc")) std::printf(" interp %05x compiled %08x", A.pc, res);
                        std::printf("\n");
                    }
                }
                if (std::memcmp(A.ls, pristine, sizeof pristine)) std::memcpy(A.ls, pristine, sizeof pristine);
                if (std::memcmp(C.ls, pristine, sizeof pristine)) std::memcpy(C.ls, pristine, sizeof pristine);
            }
        }
        std::printf("program %x: %llu instruction runs compared, %llu instructions left to the interpreter, %llu mismatches\n", cp->eaBin,
                    (unsigned long long)tested, (unsigned long long)deferred, (unsigned long long)bad);
        for (auto& kv : badByOp) std::printf("    op11 %03x: %d\n", kv.first, kv.second);
        totalBad += int(bad);
    }
    std::printf(totalBad ? "FAILED\n" : "OK: recompiled code matches the interpreter\n");
    return totalBad ? 1 : 0;
}
