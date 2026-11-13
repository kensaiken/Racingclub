#pragma once
// Runtime for statically recompiled Cell PPU code (see tools/ppc2cpp.py).
// Guest memory is a 4 GB big-endian address space; 32-bit effective addresses (PS3 ABI).
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>

namespace gti::recomp {

struct V128 { uint8_t b[16]; };
struct PPU;
extern uint32_t g_watchLo, g_watchHi;       // write watch range (debug); empty when lo >= hi
void watch_hit(PPU& c, uint32_t addr, int size);
#define RC_WATCH(a, n) do { if ((a) < g_watchHi && (a) + (n) > g_watchLo) watch_hit(*this, (a), (n)); } while (0)

struct PPU {
    uint64_t r[32]{};
    double f[32]{};
    V128 v[32]{};
    uint32_t cr = 0;
    uint64_t lr = 0, ctr = 0;
    uint32_t xer_ca = 0, xer_so = 0;
    uint8_t* mem = nullptr;           // guest address 0
    uint32_t missing = 0;             // first call target that has no translation (0 = none)
    uint32_t unimplAt = 0;
    uint32_t resvAddr = 0xFFFFFFFFu;  // lwarx/ldarx reservation (address, raw big-endian value)
    uint64_t resvRaw = 0;
    uint32_t tid = 0;                 // guest thread id (lv2)

    // ---- memory (big-endian) ----
    uint8_t rd8(uint32_t a) const { return mem[a]; }
    uint16_t rd16(uint32_t a) const { return uint16_t(mem[a] << 8 | mem[a + 1]); }
    uint32_t rd32(uint32_t a) const { uint32_t x; std::memcpy(&x, mem + a, 4); return __builtin_bswap32(x); }
    uint64_t rd64(uint32_t a) const { uint64_t x; std::memcpy(&x, mem + a, 8); return __builtin_bswap64(x); }
    void wr8(uint32_t a, uint64_t v) { RC_WATCH(a, 1); mem[a] = uint8_t(v); }
    void wr16(uint32_t a, uint64_t v) { RC_WATCH(a, 2); mem[a] = uint8_t(v >> 8); mem[a + 1] = uint8_t(v); }
    void wr32(uint32_t a, uint64_t v) { RC_WATCH(a, 4); uint32_t x = __builtin_bswap32(uint32_t(v)); std::memcpy(mem + a, &x, 4); }
    void wr64(uint32_t a, uint64_t v) { RC_WATCH(a, 8); uint64_t x = __builtin_bswap64(v); std::memcpy(mem + a, &x, 8); }
    float rdf32(uint32_t a) const { uint32_t u = rd32(a); float f; std::memcpy(&f, &u, 4); return f; }
    double rdf64(uint32_t a) const { uint64_t u = rd64(a); double d; std::memcpy(&d, &u, 8); return d; }
    void wrf32(uint32_t a, float f) { uint32_t u; std::memcpy(&u, &f, 4); wr32(a, u); }
    void wrf64(uint32_t a, double d) { uint64_t u; std::memcpy(&u, &d, 8); wr64(a, u); }

    // ---- condition register ----
    // ---- reservations: atomic on the host so several guest threads can share memory ----
    uint64_t lwarx(uint32_t a) { uint32_t raw = __atomic_load_n(reinterpret_cast<uint32_t*>(mem + a), __ATOMIC_SEQ_CST); resvAddr = a; resvRaw = raw; return __builtin_bswap32(raw); }
    uint64_t ldarx(uint32_t a) { uint64_t raw = __atomic_load_n(reinterpret_cast<uint64_t*>(mem + a), __ATOMIC_SEQ_CST); resvAddr = a; resvRaw = raw; return __builtin_bswap64(raw); }
    bool stwcx(uint32_t a, uint64_t v) {
        if (resvAddr != a) return false;
        resvAddr = 0xFFFFFFFFu;
        uint32_t expect = uint32_t(resvRaw);
        return __atomic_compare_exchange_n(reinterpret_cast<uint32_t*>(mem + a), &expect, __builtin_bswap32(uint32_t(v)), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    bool stdcx(uint32_t a, uint64_t v) {
        if (resvAddr != a) return false;
        resvAddr = 0xFFFFFFFFu;
        uint64_t expect = resvRaw;
        return __atomic_compare_exchange_n(reinterpret_cast<uint64_t*>(mem + a), &expect, __builtin_bswap64(v), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    void set_crf(int f, uint32_t v4) { const int sh = 28 - 4 * f; cr = (cr & ~(0xFu << sh)) | ((v4 & 0xF) << sh); }
    void set_crbit(int bit, uint32_t v) { const uint32_t m = 1u << (31 - bit); cr = v ? (cr | m) : (cr & ~m); }
    void cmp_s(int f, int64_t a, int64_t b) { set_crf(f, (a < b ? 8 : a > b ? 4 : 2) | (xer_so ? 1 : 0)); }
    void cmp_u(int f, uint64_t a, uint64_t b) { set_crf(f, (a < b ? 8 : a > b ? 4 : 2) | (xer_so ? 1 : 0)); }
    void fcmp(int f, double a, double b) {
        if (std::isnan(a) || std::isnan(b)) set_crf(f, 1);
        else set_crf(f, a < b ? 8 : a > b ? 4 : 2);
    }

    // ---- integer helpers ----
    static uint32_t rotl32(uint32_t x, int n) { n &= 31; return n ? (x << n) | (x >> (32 - n)) : x; }
    static uint64_t rotl64(uint64_t x, int n) { n &= 63; return n ? (x << n) | (x >> (64 - n)) : x; }
    static uint32_t mask32(int mb, int me) {
        uint32_t m = 0;
        for (int i = mb;; i = (i + 1) & 31) { m |= 1u << (31 - i); if (i == me) break; }
        return m;
    }
    static uint64_t rlw(uint32_t x, int sh, int mb, int me) { return rotl32(x, sh) & mask32(mb, me); }
    static uint64_t rlwimi(uint32_t a, uint32_t s, int sh, int mb, int me) { const uint32_t m = mask32(mb, me); return (rotl32(s, sh) & m) | (a & ~m); }
    static uint64_t rldimi(uint64_t a, uint64_t s, int sh, int mb) {
        const uint64_t m = (~0ull >> mb) & (~0ull << sh);
        return (rotl64(s, sh) & m) | (a & ~m);
    }
    uint64_t sraw(int32_t x, uint32_t n) {
        if (n > 31) { xer_ca = x < 0; return uint64_t(int64_t(x < 0 ? -1 : 0)); }
        const int32_t r = x >> n;
        xer_ca = (x < 0) && n && (uint32_t(x) & ((1u << n) - 1));
        return uint64_t(int64_t(r));
    }
    static uint64_t cntlzw(uint32_t x) { return x ? __builtin_clz(x) : 32; }

    // ---- float helpers ----
    static uint64_t fbits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }
    static double frombits(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }
    static double fctiwz(double d) {
        int64_t v;
        if (std::isnan(d)) v = int64_t(0x80000000u);
        else if (d >= 2147483647.0) v = 0x7fffffff;
        else if (d <= -2147483648.0) v = int64_t(0x80000000u);
        else v = int64_t(int32_t(d));
        return frombits(uint64_t(v) & 0xffffffffull);
    }
    static double fctidz(double d) {
        int64_t v;
        if (std::isnan(d) || d <= -9.2233720368547758e18) v = INT64_MIN;
        else if (d >= 9.2233720368547758e18) v = INT64_MAX;
        else v = int64_t(d);
        return frombits(uint64_t(v));
    }

    // ---- VMX memory ----
    V128 lvx(uint32_t a) const { V128 r; std::memcpy(r.b, mem + (a & ~15u), 16); return r; }
    void stvx(uint32_t a, const V128& v) { RC_WATCH(a & ~15u, 16); std::memcpy(mem + (a & ~15u), v.b, 16); }
    V128 lvewx(uint32_t a) const { V128 r{}; const uint32_t al = a & ~3u; std::memcpy(r.b + (al & 15), mem + al, 4); return r; }
    void stvewx(uint32_t a, const V128& v) { const uint32_t al = a & ~3u; RC_WATCH(al, 4); std::memcpy(mem + al, v.b + (al & 15), 4); }
    static V128 lvsl(uint32_t a) { V128 r; for (int i = 0; i < 16; ++i) r.b[i] = uint8_t((a & 15) + i); return r; }
    static V128 lvsr(uint32_t a) { V128 r; for (int i = 0; i < 16; ++i) r.b[i] = uint8_t(16 - (a & 15) + i); return r; }

    void unimpl(uint32_t at) { if (!unimplAt) unimplAt = at; }
};

// ---- vector lane helpers (big-endian words) ----
inline uint32_t vw(const V128& v, int i) { return uint32_t(v.b[i * 4]) << 24 | uint32_t(v.b[i * 4 + 1]) << 16 | uint32_t(v.b[i * 4 + 2]) << 8 | v.b[i * 4 + 3]; }
inline void vsetw(V128& v, int i, uint32_t x) { v.b[i * 4] = uint8_t(x >> 24); v.b[i * 4 + 1] = uint8_t(x >> 16); v.b[i * 4 + 2] = uint8_t(x >> 8); v.b[i * 4 + 3] = uint8_t(x); }
inline float vf(const V128& v, int i) { uint32_t u = vw(v, i); float f; std::memcpy(&f, &u, 4); return f; }
inline float vflush(float f) { return std::fpclassify(f) == FP_SUBNORMAL ? std::copysign(0.0f, f) : f; }   // VMX non-Java mode
inline void vsetf(V128& v, int i, float f) { f = vflush(f); uint32_t u; std::memcpy(&u, &f, 4); vsetw(v, i, u); }

#define VX_F2(name, expr) inline V128 vx_##name(const V128& a, const V128& b) { V128 r; for (int i = 0; i < 4; ++i) { float x = vflush(vf(a, i)), y = vflush(vf(b, i)); vsetf(r, i, (expr)); } return r; }
VX_F2(vaddfp, x + y)
VX_F2(vsubfp, x - y)
VX_F2(vminfp, std::fmin(x, y))
VX_F2(vmaxfp, std::fmax(x, y))
#undef VX_F2
#define VX_W2(name, expr) inline V128 vx_##name(const V128& a, const V128& b) { V128 r; for (int i = 0; i < 4; ++i) { uint32_t x = vw(a, i), y = vw(b, i); vsetw(r, i, (expr)); } return r; }
VX_W2(vxor, x ^ y)
VX_W2(vor, x | y)
VX_W2(vand, x & y)
VX_W2(vandc, x & ~y)
VX_W2(vnor, ~(x | y))
VX_W2(vadduwm, x + y)
VX_W2(vsubuwm, x - y)
VX_W2(vslw, x << (y & 31))
VX_W2(vsrw, x >> (y & 31))
VX_W2(vsraw, uint32_t(int32_t(x) >> (y & 31)))
VX_W2(vcmpequw, x == y ? 0xffffffffu : 0u)
#undef VX_W2
#define VX_C2(name, op) inline V128 vx_##name(const V128& a, const V128& b) { V128 r; for (int i = 0; i < 4; ++i) vsetw(r, i, (vflush(vf(a, i)) op vflush(vf(b, i))) ? 0xffffffffu : 0u); return r; }
VX_C2(vcmpeqfp, ==)
VX_C2(vcmpgefp, >=)
VX_C2(vcmpgtfp, >)
#undef VX_C2
inline uint32_t vx_cr6(const V128& r) {
    bool all = true, none = true;
    for (int i = 0; i < 4; ++i) { if (vw(r, i)) none = false; else all = false; }
    return (all ? 8u : 0u) | (none ? 2u : 0u);
}
inline V128 vx_vmrghw(const V128& a, const V128& b) { V128 r; vsetw(r, 0, vw(a, 0)); vsetw(r, 1, vw(b, 0)); vsetw(r, 2, vw(a, 1)); vsetw(r, 3, vw(b, 1)); return r; }
inline V128 vx_vmrglw(const V128& a, const V128& b) { V128 r; vsetw(r, 0, vw(a, 2)); vsetw(r, 1, vw(b, 2)); vsetw(r, 2, vw(a, 3)); vsetw(r, 3, vw(b, 3)); return r; }
inline V128 vx_vmaddfp(const V128& a, const V128& c, const V128& b) { V128 r; for (int i = 0; i < 4; ++i) vsetf(r, i, std::fmaf(vflush(vf(a, i)), vflush(vf(c, i)), vflush(vf(b, i)))); return r; }
inline V128 vx_vnmsubfp(const V128& a, const V128& c, const V128& b) { V128 r; for (int i = 0; i < 4; ++i) vsetf(r, i, -std::fmaf(vflush(vf(a, i)), vflush(vf(c, i)), -vflush(vf(b, i)))); return r; }
inline V128 vx_vperm(const V128& a, const V128& b, const V128& c) {
    V128 r;
    for (int i = 0; i < 16; ++i) { const int s = c.b[i] & 31; r.b[i] = s < 16 ? a.b[s] : b.b[s - 16]; }
    return r;
}
inline V128 vx_vsel(const V128& a, const V128& b, const V128& c) { V128 r; for (int i = 0; i < 16; ++i) r.b[i] = uint8_t((a.b[i] & ~c.b[i]) | (b.b[i] & c.b[i])); return r; }
inline V128 vx_vsldoi(const V128& a, const V128& b, int sh) { V128 r; for (int i = 0; i < 16; ++i) { const int s = i + sh; r.b[i] = s < 16 ? a.b[s] : b.b[s - 16]; } return r; }
inline V128 vx_vspltw(const V128& a, int i) { V128 r; const uint32_t w = vw(a, i & 3); for (int k = 0; k < 4; ++k) vsetw(r, k, w); return r; }
inline V128 vx_vspltisw(int imm) { V128 r; for (int k = 0; k < 4; ++k) vsetw(r, k, uint32_t(int32_t(imm))); return r; }
inline V128 vx_vrsqrtefp(const V128& a) { V128 r; for (int i = 0; i < 4; ++i) vsetf(r, i, 1.0f / std::sqrt(vflush(vf(a, i)))); return r; }
inline V128 vx_vrefp(const V128& a) { V128 r; for (int i = 0; i < 4; ++i) vsetf(r, i, 1.0f / vflush(vf(a, i))); return r; }
inline V128 vx_vctsxs(const V128& a, int sh) {
    V128 r;
    for (int i = 0; i < 4; ++i) {
        double d = double(vf(a, i)) * double(1u << sh);
        int32_t x = std::isnan(d) ? 0 : d >= 2147483647.0 ? 0x7fffffff : d <= -2147483648.0 ? int32_t(0x80000000u) : int32_t(d);
        vsetw(r, i, uint32_t(x));
    }
    return r;
}
inline V128 vx_vcfsx(const V128& a, int sh) { V128 r; for (int i = 0; i < 4; ++i) vsetf(r, i, float(int32_t(vw(a, i))) / float(1u << sh)); return r; }

// ---- dispatch ----
using Fn = void (*)(PPU&);
struct FuncEntry { uint32_t addr; Fn fn; };
extern const FuncEntry kFuncs[];
extern const size_t kFuncCount;
using HostHook = bool (*)(PPU&, uint32_t addr);   // returns true if it handled the call (stub)
extern HostHook g_hostHook;
void rc_mark_hooked(uint32_t addr);   // calls to addr must go through g_hostHook (system library stubs)
void rc_call(PPU& c, uint32_t addr);
uint64_t rc_timebase();                    // PS3 time base counter (79.8 MHz)
using SyscallHook = void (*)(PPU&);
extern SyscallHook g_syscallHook;          // lv2 syscalls (sc); unset = reported as unimplemented
void rc_syscall(PPU& c);

} // namespace gti::recomp
