#pragma once
// Runtime for SPU programs statically recompiled to C++ by tools/spu2cpp.py.
// Registers are kept as four host-order 32-bit words (word 0 = preferred slot); bytes and halfwords are addressed
// in the SPU's big-endian numbering through B()/H(). Semantics mirror the interpreter in spu.cpp exactly.
#include "recomp/spu.h"
#include <cmath>
#include <cstring>

namespace gti::recomp::spurt {

struct Q { uint32_t w[4]; };
using u128 = unsigned __int128;

inline uint8_t& B(Q& q, int i) { return reinterpret_cast<uint8_t*>(q.w)[i ^ 3]; }
inline uint8_t b(const Q& q, int i) { return reinterpret_cast<const uint8_t*>(q.w)[i ^ 3]; }
inline uint16_t& H(Q& q, int i) { return reinterpret_cast<uint16_t*>(q.w)[i ^ 1]; }
inline uint16_t h(const Q& q, int i) { return reinterpret_cast<const uint16_t*>(q.w)[i ^ 1]; }
inline float f(const Q& q, int i) { float x; std::memcpy(&x, &q.w[i], 4); return x; }
inline uint32_t fb(float x) { uint32_t u; std::memcpy(&u, &x, 4); return u; }
inline double d(const Q& q, int i) { const uint64_t u = uint64_t(q.w[2 * i]) << 32 | q.w[2 * i + 1]; double x; std::memcpy(&x, &u, 8); return x; }
inline void sd(Q& q, int i, double x) { uint64_t u; std::memcpy(&u, &x, 8); q.w[2 * i] = uint32_t(u >> 32); q.w[2 * i + 1] = uint32_t(u); }
inline uint64_t dw(const Q& q, int i) { return uint64_t(q.w[2 * i]) << 32 | q.w[2 * i + 1]; }
inline void sdw(Q& q, int i, uint64_t u) { q.w[2 * i] = uint32_t(u >> 32); q.w[2 * i + 1] = uint32_t(u); }
inline u128 q128(const Q& q) { return u128(q.w[0]) << 96 | u128(q.w[1]) << 64 | u128(q.w[2]) << 32 | q.w[3]; }
inline Q from128(u128 x) { Q q; q.w[0] = uint32_t(x >> 96); q.w[1] = uint32_t(x >> 64); q.w[2] = uint32_t(x >> 32); q.w[3] = uint32_t(x); return q; }
inline Q ldq(const uint8_t* ls, uint32_t a) {
    a &= 0x3fff0;
    Q q;
    for (int k = 0; k < 4; ++k) { uint32_t v; std::memcpy(&v, ls + a + 4 * k, 4); q.w[k] = __builtin_bswap32(v); }
    return q;
}
inline void stq(uint8_t* ls, uint32_t a, const Q& q) {
    a &= 0x3fff0;
    for (int k = 0; k < 4; ++k) { const uint32_t v = __builtin_bswap32(q.w[k]); std::memcpy(ls + a + 4 * k, &v, 4); }
}
inline Q splat(uint32_t v) { Q q; q.w[0] = q.w[1] = q.w[2] = q.w[3] = v; return q; }
inline Q pref(uint32_t v) { Q q{}; q.w[0] = v; return q; }
inline Q shufb(const Q& a, const Q& bb, const Q& c) {
    Q o;
    for (int i = 0; i < 16; ++i) {
        const uint8_t s = b(c, i);
        B(o, i) = (s & 0xC0) == 0x80 ? 0x00 : (s & 0xE0) == 0xC0 ? 0xFF : (s & 0xE0) == 0xE0 ? 0x80 : ((s & 0x10) ? b(bb, s & 0xF) : b(a, s & 0xF));
    }
    return o;
}
inline Q insmask(uint32_t addr, int size) {   // cbd/chd/cwd/cdd family
    Q o;
    for (int i = 0; i < 16; ++i) B(o, i) = uint8_t(0x10 + i);
    const int t = int(addr & 0xF) & ~(size - 1);
    if (size == 1) B(o, t) = 3;
    else if (size == 2) { B(o, t) = 2; B(o, t + 1) = 3; }
    else for (int k = 0; k < size; ++k) B(o, t + k) = uint8_t(k);
    return o;
}
inline Q qshl_bytes(const Q& a, int n) { Q o; for (int i = 0; i < 16; ++i) B(o, i) = i + n < 16 ? b(a, i + n) : 0; return o; }
inline Q qshr_bytes(const Q& a, int n) { Q o; for (int i = 0; i < 16; ++i) B(o, i) = i - n >= 0 ? b(a, i - n) : 0; return o; }
inline Q qrot_bytes(const Q& a, int n) { Q o; for (int i = 0; i < 16; ++i) B(o, i) = b(a, (i + n) & 15); return o; }
inline uint32_t cflts(float x, int i8) { const double v = std::ldexp(double(x), 173 - i8); return uint32_t(v >= 2147483647.0 ? 0x7fffffff : v <= -2147483648.0 ? int32_t(0x80000000u) : int32_t(v)); }
inline uint32_t cfltu(float x, int i8) { const double v = std::ldexp(double(x), 173 - i8); return v >= 4294967295.0 ? 0xffffffffu : v <= 0 ? 0u : uint32_t(v); }

// SPUPROF=1: trampoline entries per (program, chunk), printed by the job manager
extern bool g_spuProf;
extern uint64_t g_spuProfCnt[64][256];

// one recompiled program: runs from s.pc until it needs the interpreter (unknown target, stop, halt, ...)
struct Compiled { uint32_t eaBin, lsBase, size; void (*fn)(Spu&); };
const Compiled* find_compiled(uint32_t eaBin);

} // namespace gti::recomp::spurt
