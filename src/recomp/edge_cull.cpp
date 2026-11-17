// Native replacement of Edge Geometry's triangle culling routine (SPU program at EA 0xbaea80, LS 0x9e68, called
// from 0x43fc, returns to 0x4400). Rules measured on captured jobs (CULLCAP, tools: cull_replay):
//   per vertex  c = M * L * p (input 15: 4x4 view-projection, input 16: 3x4 local-to-world)
//               outcode bits: 0x04 z > w, 0x80 z < 0, and for w > 0 with screen = c.xy / w * scale + offset:
//               0x08 y > height, 0x10 x > width, 0x20 y < 0, 0x40 x < 0
//               snapped screen coordinates: trunc(8 v) & ~1, 16 bits
//   triangle    all three share an outcode bit -> culled; any vertex with z < 0 -> kept;
//               mode 3 (r4): float area < 0 -> culled (back face); three equal snapped x or three equal snapped y ->
//               culled; otherwise kept.
// Our arithmetic is not the SPU's (reciprocal estimate, float32 order), so every test that is too close to call
// keeps the triangle: the result is a superset of the console's (a few more slivers drawn, never a missing one).
#include "recomp/edge_cull.h"
#include <cmath>
#include <x86intrin.h>
#include <cstring>

namespace gti::recomp {

uint64_t g_edgeCullCyc[4];   // profiling: load, vertices, triangles
uint8_t* g_edgeCullWhy;      // debug (cull_replay): why each kept triangle was kept

namespace {
inline uint32_t be32(const uint8_t* ls, uint32_t a) { const uint8_t* p = ls + (a & 0x3ffff); return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
inline uint16_t be16(const uint8_t* ls, uint32_t a) { const uint8_t* p = ls + (a & 0x3ffff); return uint16_t(p[0] << 8 | p[1]); }
inline float bef(const uint8_t* ls, uint32_t a) { const uint32_t u = be32(ls, a); float f; std::memcpy(&f, &u, 4); return f; }
inline void wr32(uint8_t* ls, uint32_t a, uint32_t v) { uint8_t* p = ls + (a & 0x3ffff); p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }

}  // namespace

EdgeCullResult edge_cull_native(uint8_t* ls, uint32_t ctx, uint32_t mode, uint32_t entryR1) {
    EdgeCullResult res{};
    if (mode != 1 && mode != 3) return res;
    ctx &= 0x3fff0;
    const uint32_t w2 = be32(ls, ctx + 8);
    const uint32_t nv = w2 >> 16, ni = w2 & 0xffff;
    const uint32_t pos = be32(ls, ctx + 0x10), idx = be32(ls, ctx + 0x50);
    if (ni % 3 || nv == 0 || nv > 0x2000 || pos + nv * 16 > 0x40000 || idx + ni * 2 > 0x40000) return res;
    // job inputs: gathered after the binary in DMA list order, 16-byte aligned, keeping the EA's low 4 bits
    uint32_t inLs[18] = {}, inSz[18] = {};
    {
        const uint32_t n = be16(ls, 0x2d0a) / 8;
        if (n < 18) return res;
        uint32_t off = be32(ls, 0x2c00);
        for (uint32_t k = 0; k < 18; ++k) {
            const uint32_t sz = be32(ls, 0x2d30 + 8 * k) & 0x7fff, ea = be32(ls, 0x2d34 + 8 * k);
            inLs[k] = off | (ea & 15); inSz[k] = sz;
            off += (sz + 15) & ~15u;
        }
    }
    if (inSz[15] != 0x80 || inSz[16] != 0x30) return res;
    const uint32_t vp = inLs[15], lw = inLs[16];
    const unsigned sx0 = be16(ls, vp), sy0 = be16(ls, vp + 2), sw = be16(ls, vp + 4), sh = be16(ls, vp + 6);
    const double scX = bef(ls, vp + 0x50), scY = bef(ls, vp + 0x54), ofX = bef(ls, vp + 0x60), ofY = bef(ls, vp + 0x64);
    // only the viewport layout seen in this game (full-target scissor, scale/offset mapping clip space onto it)
    if (sx0 || sy0 || !sw || !sh || scX != sw * 0.5 || scY != -(sh * 0.5) || ofX != sw * 0.5 || ofY != sh * 0.5) return res;
    double M[4][4], L[3][4], T[4][4], TA[4][4];   // TA: |M| |L|, magnitude of the products the SPU rounds
    for (int i = 0; i < 4; ++i) for (int k = 0; k < 4; ++k) M[i][k] = bef(ls, vp + 0x10 + 16 * i + 4 * k);
    for (int i = 0; i < 3; ++i) for (int k = 0; k < 4; ++k) L[i][k] = bef(ls, lw + 16 * i + 4 * k);
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k) {
            double v = 0, va = 0;
            for (int j = 0; j < 3; ++j) { v += M[i][j] * L[j][k]; va += std::fabs(M[i][j]) * std::fabs(L[j][k]); }
            if (k == 3) { v += M[i][3]; va += std::fabs(M[i][3]); }
            T[i][k] = v; TA[i][k] = va;
        }
    // per-vertex pass, 8 vertices at a time in float (separate arrays, no branches). The error bounds (3e-7 of the
    // magnitudes involved) cover both the SPU's float32 roundings and ours.
    struct Soa { float x[0x2000], y[0x2000], e[0x2000], p[4][0x2000]; int16_t qx[0x2000], qy[0x2000]; uint8_t f[0x2000]; };
    static thread_local Soa soaStore;
    Soa& V = soaStore;   // one TLS lookup (MinGW emulates thread_local with a call per access)
    uint64_t q0 = __rdtsc();
    {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(ls + pos);   // 16-byte aligned float4 per vertex
#pragma clang loop vectorize(assume_safety)
        for (uint32_t v = 0; v < nv; ++v) {
            const uint32_t u0 = __builtin_bswap32(src[4 * v]), u1 = __builtin_bswap32(src[4 * v + 1]),
                           u2 = __builtin_bswap32(src[4 * v + 2]), u3 = __builtin_bswap32(src[4 * v + 3]);
            std::memcpy(&V.p[0][v], &u0, 4); std::memcpy(&V.p[1][v], &u1, 4); std::memcpy(&V.p[2][v], &u2, 4); std::memcpy(&V.p[3][v], &u3, 4);
        }
    }
    uint64_t q1 = __rdtsc(); g_edgeCullCyc[0] += q1 - q0;
    float Tf[4][4], TAf[4][4];
    for (int i = 0; i < 4; ++i) for (int k = 0; k < 4; ++k) { Tf[i][k] = float(T[i][k]); TAf[i][k] = float(TA[i][k]); }
    const float fScX = float(scX), fScY = float(scY), fOfX = float(ofX), fOfY = float(ofY);
    const float aScX = std::fabs(fScX), aScY = std::fabs(fScY), fW = float(sw), fH = float(sh);
#pragma clang loop vectorize(assume_safety) vectorize_width(8)
    for (uint32_t v = 0; v < nv; ++v) {
        const float p0 = V.p[0][v], p1 = V.p[1][v], p2 = V.p[2][v], p3 = V.p[3][v];
        const float a0 = std::fabs(p0), a1 = std::fabs(p1), a2 = std::fabs(p2), a3 = std::fabs(p3);
        float c[4], m[4];
        for (int i = 0; i < 4; ++i) {
            const float t0 = Tf[i][0] * p0, t1 = Tf[i][1] * p1, t2 = Tf[i][2] * p2, t3 = Tf[i][3] * p3;
            c[i] = (t0 + t1) + (t2 + t3);
            // error bound: the SPU may round the matrix product M L and the world position as well as the sums
            m[i] = ((TAf[i][0] * a0 + TAf[i][1] * a1) + (TAf[i][2] * a2 + TAf[i][3] * a3)) * 3e-7f + 1e-30f;
        }
        const bool screen = c[3] > m[3] * 4 && c[3] > 1e-20f;
        const float iw = 1.0f / (screen ? c[3] : 1.0f), rw = m[3] * iw;
        const float x = c[0] * iw * fScX + fOfX, y = c[1] * iw * fScY + fOfY;
        // propagated error + the SPU's reciprocal estimate and float32 roundings (relative ~1e-6)
        const float e = (m[0] * iw + std::fabs(c[0] * iw) * rw) * aScX + (m[1] * iw + std::fabs(c[1] * iw) * rw) * aScY
                      + (std::fabs(x) + std::fabs(y) + 1.0f) * 6e-7f;
        int f = (c[2] - c[3] > m[2] + m[3] ? 0x04 : 0) | (c[2] < -m[2] ? 0x80 : 0);
        f |= screen ? ((y > fH + e ? 0x08 : 0) | (x > fW + e ? 0x10 : 0) | (y < -e ? 0x20 : 0) | (x < -e ? 0x40 : 0)) : 0;
        const bool noArea = c[2] <= m[2] || !screen;   // z < 0 not ruled out, or no screen position: keep
        // snapped coordinates as the SPU computes them: cflts (scale 8, toward zero) then & ~1, low 16 bits
        const float x8 = x * 8.0f, y8 = y * 8.0f, e8 = e * 8.0f + 1e-6f;
        const float tx = std::trunc(x8), ty = std::trunc(y8);
        const bool snapOk = std::fabs(x8) < 2.0e9f && std::fabs(y8) < 2.0e9f && std::trunc(x8 - e8) == tx && std::trunc(x8 + e8) == tx
                         && std::trunc(y8 - e8) == ty && std::trunc(y8 + e8) == ty;
        V.qx[v] = int16_t(int32_t(snapOk ? tx : 0.0f) & ~1);
        V.qy[v] = int16_t(int32_t(snapOk ? ty : 0.0f) & ~1);
        V.x[v] = x; V.y[v] = y; V.e[v] = e;
        V.f[v] = uint8_t(f | (noArea ? 1 : 0) | (noArea || !snapOk ? 2 : 0));
    }
    uint64_t q2 = __rdtsc(); g_edgeCullCyc[1] += q2 - q1;
    // triangles, compacted in place
    uint32_t kept = 0;
    for (uint32_t t = 0; t < ni; t += 3) {
        const uint16_t i0 = be16(ls, idx + 2 * t), i1 = be16(ls, idx + 2 * t + 2), i2 = be16(ls, idx + 2 * t + 4);
        if (i0 >= nv || i1 >= nv || i2 >= nv) return EdgeCullResult{};
        const uint8_t fa = V.f[i0], fb = V.f[i1], fc = V.f[i2];
        bool cull = (fa & fb & fc & 0xfc) != 0;
        uint8_t why = 0;   // 1 no area test (near / no screen position), 2 back face too close, 4 snap uncertain, 0 passed
        if (!cull && ((fa | fb | fc) & 1)) why = 1;
        if (!cull && !((fa | fb | fc) & 1)) {
            const double ax = V.x[i0], ay = V.y[i0], bx = V.x[i1], by = V.y[i1], cx = V.x[i2], cy = V.y[i2];   // area in double
            if (mode == 3) {
                const double t1 = (bx - ax) * (cy - ay), t2 = (cx - ax) * (by - ay);
                const double e = std::fmax(V.e[i0], std::fmax(V.e[i1], V.e[i2])) + (std::fabs(ax) + std::fabs(ay) + std::fabs(bx) + std::fabs(by) + std::fabs(cx) + std::fabs(cy)) * 1e-7;
                const double ea = (std::fabs(t1) + std::fabs(t2)) * 1e-6
                    + 2 * e * (std::fabs(bx - ax) + std::fabs(cx - ax) + std::fabs(by - ay) + std::fabs(cy - ay)) + 4 * e * e;
                if (t1 - t2 < -ea) cull = true;
                else if (t1 - t2 < ea) why |= 2;
            }
            if (!cull && !((fa | fb | fc) & 2)
                && ((V.qx[i0] == V.qx[i1] && V.qx[i0] == V.qx[i2]) || (V.qy[i0] == V.qy[i1] && V.qy[i0] == V.qy[i2]))) cull = true;
        }
        if (!cull && ((fa | fb | fc) & 2) && !why) why = 4;
        if (!cull) {
            if (g_edgeCullWhy) g_edgeCullWhy[kept / 3] = why;
            uint8_t* d = ls + ((idx + 2 * kept) & 0x3ffff);
            d[0] = uint8_t(i0 >> 8); d[1] = uint8_t(i0); d[2] = uint8_t(i1 >> 8); d[3] = uint8_t(i1); d[4] = uint8_t(i2 >> 8); d[5] = uint8_t(i2);
            kept += 3;
        }
    }
    g_edgeCullCyc[2] += __rdtsc() - q2;
    wr32(ls, ctx + 0x58, idx + ((kept * 2 + 15) & ~15u));
    wr32(ls, entryR1 + 0x258, kept);
    res.handled = true;
    res.count = kept;
    return res;
}

}  // namespace gti::recomp
