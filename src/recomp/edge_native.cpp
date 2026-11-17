// Native replacements of Edge Geometry routines: see edge_native.h. Verified against captured jobs with
// FNCAP + build-test/fn_replay.exe (NATIVE=1), which compares the whole local store outside the routine's stack frame.
#include "recomp/edge_native.h"
#include <cstdlib>
#include <cstring>

namespace gti::recomp {

uint64_t g_edgeNativeCalls[4], g_edgeNativeFallbacks[4];   // per routine (0: 0x7140)

namespace {
inline uint32_t be32(const uint8_t* ls, uint32_t a) { const uint8_t* p = ls + (a & 0x3ffff); return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
inline uint16_t be16(const uint8_t* ls, uint32_t a) { const uint8_t* p = ls + (a & 0x3ffff); return uint16_t(p[0] << 8 | p[1]); }
inline void wr32(uint8_t* ls, uint32_t a, uint32_t v) { uint8_t* p = ls + (a & 0x3ffff); p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }

// LS 0x7140 (called from 0x4524, after the culling): writes the vertex positions for the RSX after the index list.
// Output format 0 (byte 4 of the context) is the only one this game uses: x, y, z float of each vertex packed to
// 12 bytes, from the stream whose id byte in the table at ctx+0x160 is 1 (stream pointers at ctx+0x10..).
// The SPU code works on vertex pairs (count rounded up to even); behind the data its last store leaves 8 zero bytes
// and the y, z of the last vertex. Context: +0x54 = start of the data, +0x58 = end (the routine's r3 = start).
bool out_positions(uint8_t* ls, uint32_t ctxp, uint32_t* ret) {
    const uint32_t ctx = ctxp & 0x3fff0;
    if (ctx + 0x170 > 0x40000) return false;
    if (ls[ctx] & 0x10) return false;          // flag the SPU code checks first: nothing to write
    if (ls[ctx + 4] != 0) return false;        // other output formats
    const uint32_t nv = be16(ls, ctx + 8);
    if (nv == 0 || (nv & 1)) return false;
    int k = -1;
    for (int i = 0; i < 16; ++i) if (ls[ctx + 0x160 + i] == 1) { k = i; break; }
    if (k < 0) return false;
    const uint32_t src = be32(ls, ctx + 0x10 + 4 * uint32_t(k)), dst = be32(ls, ctx + 0x58);
    if ((src | dst) & 15 || src >= 0x40000 || dst >= 0x40000) return false;
    const uint32_t end = dst + 12 * nv;
    if (uint64_t(src) + 16ull * nv > 0x40000 || end + 16 > 0x40000) return false;
    if (dst > src && dst < src + 16 * nv) return false;   // the packed data would overrun unread input
    if (src > dst && src < end + 16) {
        // input behind the output: a forward copy is safe as long as every vertex is read before it is overwritten
        // (12-byte writes, 16-byte reads: the write position never passes the read position when dst <= src)
    }
    uint8_t lastYZ[8];
    std::memcpy(lastYZ, ls + src + 16 * (nv - 1) + 4, 8);
    uint8_t* o = ls + dst;
    const uint8_t* in = ls + src;
    for (uint32_t v = 0; v < nv; ++v) std::memmove(o + 12 * v, in + 16 * v, 12);
    std::memset(ls + end, 0, 8);
    std::memcpy(ls + end + 8, lastYZ, 8);
    wr32(ls, ctx + 0x54, dst);
    wr32(ls, ctx + 0x58, end);
    *ret = dst;
    return true;
}
}  // namespace

bool edge_native_run(uint32_t entry, uint8_t* ls, const uint32_t* a, uint32_t r1, uint32_t* ret) {
    (void)r1;
    static const bool off = std::getenv("EDGENATIVE2") && std::atoi(std::getenv("EDGENATIVE2")) == 0;
    if (off) return false;
    switch (entry) {
        case 0x7140: {
            const bool ok = out_positions(ls, a[0], ret);
            ++(ok ? g_edgeNativeCalls[0] : g_edgeNativeFallbacks[0]);
            return ok;
        }
        default: return false;
    }
}

} // namespace gti::recomp
