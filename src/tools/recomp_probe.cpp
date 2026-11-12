// Calls every virtual method of a few objects on a fresh copy of a snapshot and reports which ones change the
// car's committed state (pos +0x90, vel +0x140, angvel +0x150).
// usage: recomp_probe EBOOT.elf snapshot car_hex
#include "recomp/guest.h"
#include "recomp/hle.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace gti::recomp;
namespace gti::recomp { extern bool g_shadowOn; void rc_dump_shadow(); extern uint64_t g_timebaseFixed; }
#include <windows.h>
static void rc_call_direct(PPU& c, uint32_t addr) {   // dispatch without the host hook
    const FuncEntry* b = kFuncs;
    const FuncEntry* e = kFuncs + kFuncCount;
    const FuncEntry* it = std::lower_bound(b, e, addr, [](const FuncEntry& f, uint32_t a) { return f.addr < a; });
    if (it != e && it->addr == addr) it->fn(c);
    else if (!c.missing) c.missing = addr;
}
static LONG WINAPI crash_filter(EXCEPTION_POINTERS* e) {
    std::printf("\n[crash] image rva 0x%llx\n", (unsigned long long)((uint8_t*)e->ExceptionRecord->ExceptionAddress - (uint8_t*)GetModuleHandleA(nullptr)));
    std::printf("\n[crash] code 0x%lx at host %p addr 0x%llx\n", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress,
                (unsigned long long)(e->ExceptionRecord->NumberParameters > 1 ? e->ExceptionRecord->ExceptionInformation[1] : 0));
    rc_dump_shadow();
    return EXCEPTION_EXECUTE_HANDLER;
}

static void sys_stub(PPU& c) { c.r[3] = 0; }

// Rigid-body integration measured on the original (the game's own routine is not located yet):
// COM += vel*dt; every orientation row rotated about angvel by |angvel|*dt; origin follows the COM.
static void integrate(PPU& c, uint32_t car, float dt) {
    // the commit (0x313a70) has already rotated the orientation (+0x20 block); the position step copies that
    // rotation into the +0x60 block and advances origin (+0x50) and centre of mass (+0x90) by vel*dt
    for (int i = 0; i < 12; ++i) c.wr32(car + 0x60 + 4 * i, c.rd32(car + 0x20 + 4 * i));
    for (int i = 0; i < 3; ++i) {
        const float v = c.rdf32(car + 0x140 + 4 * i);
        c.wrf32(car + 0x90 + 4 * i, c.rdf32(car + 0x90 + 4 * i) + v * dt);
        c.wrf32(car + 0x50 + 4 * i, c.rdf32(car + 0x50 + 4 * i) + v * dt);
    }
}

int main(int argc, char** argv) {
    if (argc < 4) return 1;
    Guest g;
    g.stackTop = 0xEF000000;
    if (!g.init(argv[1]) || !g.loadSnapshot(argv[2])) { std::printf("load failed\n"); return 1; }
    const uint32_t car = uint32_t(std::strtoul(argv[3], nullptr, 16));
    hle_install();
    if (const char* tb = std::getenv("TBFIXED")) g_timebaseFixed = std::strtoull(tb, nullptr, 10);
    if (std::getenv("SHADOW")) {
        std::printf("guest mem at host %p\n", (void*)g.mem);
        static HostHook inner = g_hostHook;
        static HostHook mine = [](PPU& c, uint32_t addr) -> bool {
            if (addr == 0x99c930 && (uint64_t(uint32_t(c.r[3])) + uint32_t(c.r[5]) > 0xffffffffull || uint64_t(uint32_t(c.r[4])) + uint32_t(c.r[5]) > 0xffffffffull || uint32_t(c.r[5]) > 0x1000000)) {
                std::printf("[memcpy] bad dst 0x%llx src 0x%llx n 0x%llx lr 0x%llx\n", (unsigned long long)c.r[3], (unsigned long long)c.r[4], (unsigned long long)c.r[5], (unsigned long long)c.lr);
                rc_dump_shadow();
            }
            static int bypass = 0;
            if (bypass == 0 && std::getenv("CHECKNV")) {   // every call: verify callee-saved r14-r31, r1, f14-f31 are preserved
                uint64_t sv[32]; double fv[32];
                std::memcpy(sv, c.r, sizeof sv); std::memcpy(fv, c.f, sizeof fv);
                const uint64_t lr = c.lr;
                bypass = 1;
                const bool handled = inner(c, addr);
                bypass = 0;
                if (!handled) {
                    rc_call_direct(c, addr);
                }
                for (int r = 14; r < 32; ++r)
                    if ((addr < 0x973000 || addr >= 0x975000) && (c.r[r] != sv[r] || (r == 14 && c.r[1] != sv[1]))) {
                        static int shown = 0;
                        if (shown++ < 10) std::printf("[nv] call 0x%x (lr 0x%llx) changed r%d 0x%llx -> 0x%llx (r1 0x%llx -> 0x%llx)\n", addr, (unsigned long long)lr, r,
                                                      (unsigned long long)sv[r], (unsigned long long)c.r[r], (unsigned long long)sv[1], (unsigned long long)c.r[1]);
                        break;
                    }
                return true;
            }
            if (addr != 0xa7d4b8 && addr != 0xa7d4c8) return inner(c, addr);
            const uint64_t a = c.r[3], r2 = c.r[2];
            c.r[2] = r2 + 0x10000 - 0x1a8;                          // what the trampoline does before branching
            rc_call(c, addr == 0xa7d4b8 ? 0x48f778 : 0x48f6e8);
            c.r[2] = r2;
            std::printf("[%s] 0x%llx -> 0x%llx\n", addr == 0xa7d4b8 ? "new" : "delete", (unsigned long long)a, (unsigned long long)c.r[3]);
            return true;
        };
        g_hostHook = mine;
    }
    g_syscallHook = sys_stub;
    if (std::getenv("SHADOW")) { g_shadowOn = true; SetUnhandledExceptionFilter(crash_filter); AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS* e) -> LONG { if (e->ExceptionRecord->ExceptionCode == 0xC0000005 || e->ExceptionRecord->ExceptionCode == 0xC00000FD) crash_filter(e); return EXCEPTION_CONTINUE_SEARCH; }); }
    setvbuf(stdout, nullptr, _IONBF, 0);
    // keep a pristine copy of the low 1 GB of guest memory (code, data, heaps) to restore between probes
    const size_t keep = 0x40000000;
    std::vector<uint8_t> pristine(g.mem, g.mem + keep);
    std::vector<uint8_t> stacks(g.mem + 0xd0000000, g.mem + 0xd0400000);
    PPU c0;
    g.prepare(c0);
    struct Obj { const char* name; uint32_t addr; };
    std::vector<Obj> objs = {{"car", car}, {"car+0x2b4", c0.rd32(car + 0x2b4)}, {"car+0x37c", c0.rd32(car + 0x37c)},
                             {"car+0xb4", c0.rd32(car + 0xb4)}, {"wheel0", c0.rd32(car + 0x2c8)}};
    auto st = [&](PPU& c) {
        float v[13];
        for (int i = 0; i < 3; ++i) { v[i] = c.rdf32(car + 0x90 + i * 4); v[3 + i] = c.rdf32(car + 0x140 + i * 4); v[6 + i] = c.rdf32(car + 0x150 + i * 4); }
        v[9] = c.rdf32(car + 0x34); v[10] = c.rdf32(car + 0xf8); v[11] = c.rdf32(car + 0x188); v[12] = c.rdf32(car + 0x54);
        return std::vector<float>(v, v + 13);
    };
    const auto s0 = st(c0);
    std::printf("start pos %.4f %.4f %.4f vel %.4f %.4f %.4f\n", s0[0], s0[1], s0[2], s0[3], s0[4], s0[5]);
    if (argc > 4 && std::string(argv[4]) == "seq") {   // N frames of update + commit, compared against the emulator frames
        FILE* nf = std::fopen((std::string(argv[2]) + ".next").c_str(), "rb");
        std::vector<uint8_t> nx;
        if (nf) { uint8_t buf[65536]; size_t k; while ((k = std::fread(buf, 1, sizeof buf, nf)) > 0) nx.insert(nx.end(), buf, buf + k); std::fclose(nf); }
        const size_t stride = 8 + 0x1000;
        const size_t frames = nx.size() > 16 ? (nx.size() - 16) / stride : 0;
        auto ef = [&](size_t i, uint32_t off) { const uint8_t* b = nx.data() + 16 + i * stride + 8 + off; uint32_t u = uint32_t(b[0]) << 24 | b[1] << 16 | b[2] << 8 | b[3]; float f; std::memcpy(&f, &u, 4); return f; };
        for (size_t i = 0; i < frames && i < 12; ++i)
            std::printf("emu  %2zu pos %9.4f %9.4f %9.4f vel %8.4f %8.4f %8.4f\n", i, ef(i, 0x90), ef(i, 0x94), ef(i, 0x98), ef(i, 0x140), ef(i, 0x144), ef(i, 0x148));
        PPU c;
        for (int fr = 1; fr <= 10; ++fr) {
            {   // raw pad inputs of the emulator for this step (+0x3f0..+0x430), so both cars get the same controls
                const size_t ri = std::min<size_t>(frames - 1, size_t(2 * fr - 1));
                const uint8_t* src = nx.data() + 16 + ri * stride + 8 + 0x3f0;
                std::memcpy(g.mem + car + 0x3f0, src, 0x40);
            }
            static const uint32_t engFn = std::getenv("ENGFN") ? uint32_t(std::strtoul(std::getenv("ENGFN"), nullptr, 16)) : 0;
            if (engFn && fr > 1) { g.prepare(c); c.r[2] = 0xc7f068; c.r[3] = c.rd32(car + 0xb4); c.f[1] = 1.0 / 60.0; rc_call(c, engFn); }
            g.prepare(c); c.r[3] = car; c.f[1] = 1.0 / 60.0; rc_call(c, 0x3367c0);
            g.prepare(c); c.r[3] = car; c.f[1] = 1.0 / 60.0; rc_call(c, 0x313a70);
            integrate(c, car, 1.0f / 60.0f);
            if (fr == 1 && frames > 2) {
                const uint8_t* e2 = nx.data() + 16 + 2 * stride + 8;
                int nd = 0;
                for (uint32_t o = 0; o < 0x1000; o += 4)
                    if (std::memcmp(e2 + o, c.mem + car + o, 4) != 0) {
                        uint32_t ue = uint32_t(e2[o]) << 24 | e2[o + 1] << 16 | e2[o + 2] << 8 | e2[o + 3]; float fe; std::memcpy(&fe, &ue, 4);
                        if (nd++ < 80) std::printf("   diff +%03x port %12.5f emu %12.5f\n", o, c.rdf32(car + o), fe);
                    }
                std::printf("   differing words after step 1: %d\n", nd);
            }
            const auto s1 = st(c);
            std::printf("port %2d pos %9.4f %9.4f %9.4f vel %8.4f %8.4f %8.4f %s%s\n", fr, s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], c.missing ? "missing" : "", c.unimplAt ? "unimpl" : "");
        }
        return 0;
    }
    if (argc > 6 && std::string(argv[4]) == "replay") {   // replay <W> <rec.bin>: emulator recording (rpcs3_test) from this snapshot
        const uint32_t W = uint32_t(std::strtoul(argv[5], nullptr, 16));
        FILE* rf = std::fopen(argv[6], "rb");
        std::vector<uint8_t> d;
        if (rf) { uint8_t buf[65536]; size_t k; while ((k = std::fread(buf, 1, sizeof buf, rf)) > 0) d.insert(d.end(), buf, buf + k); std::fclose(rf); }
        uint32_t nobj = 0, N = 0;
        std::memcpy(&nobj, d.data(), 4); std::memcpy(&N, d.data() + 4, 4);
        const size_t hdr = 12 + 4 * nobj, stride = 8 + size_t(nobj) * N;
        const size_t R = d.size() > hdr ? (d.size() - hdr) / stride : 0;
        auto rec = [&](size_t j) { return d.data() + hdr + j * stride + 8; };   // object 0 = the car
        auto bf = [&](const uint8_t* b, uint32_t o) { uint32_t u = uint32_t(b[o]) << 24 | b[o + 1] << 16 | b[o + 2] << 8 | b[o + 3]; float f; std::memcpy(&f, &u, 4); return f; };
        std::printf("records %zu\n", R);
        PPU c;
        g.prepare(c);
        size_t j = 0;   // last matched record
        int exact = 0, steps = 0, firstBad = -1;
        double worst = 0;
        auto same = [&](const uint8_t* b) { return std::memcmp(b + 0xf0, c.mem + car + 0xf0, 12) == 0 && std::memcmp(b + 0x140, c.mem + car + 0x140, 24) == 0; };
        // the car rests on the grid until the recording's first input: identical idle steps
        while (j + 1 < R && std::memcmp(rec(j + 1) + 0xf0, rec(0) + 0xf0, 12) == 0) ++j;
        {   // the emulator kept running between the snapshot and the start of the recording (car clock +0x374)
            const int idle = int(std::lround((bf(rec(j), 0x374) - c.rdf32(car + 0x374)) * 60.0));
            std::printf("idle steps before the recording: %d\n", idle);
            for (int k = 0; k < idle && k < 100000; ++k) { g.prepare(c); c.r[3] = W; c.f[1] = 1.0 / 60.0; rc_call(c, 0x31bab8); }
        }
        // guest memory that a step can modify (code/data/heaps, recompiler heaps, thread stacks) for trying alternatives
        struct Span { uint32_t lo, hi; };
        const Span spans[] = {{0x00010000u, 0x40000000u}, {0x60000000u, 0x80000000u}, {0xD0000000u, 0xD0400000u}};
        std::vector<std::vector<uint8_t>> saved;
        auto save = [&] { saved.clear(); for (auto& sp : spans) saved.emplace_back(g.mem + sp.lo, g.mem + sp.hi); };
        auto restore = [&] { for (size_t k = 0; k < saved.size(); ++k) std::memcpy(g.mem + spans[k].lo, saved[k].data(), saved[k].size()); };
        auto setIn = [&](const uint8_t* ip) { for (uint32_t o : {0x410u, 0x414u, 0x418u, 0x41cu, 0x420u, 0x424u}) c.wr32(car + o, uint32_t(ip[o]) << 24 | ip[o + 1] << 16 | ip[o + 2] << 8 | ip[o + 3]); };
        // frame time handed to World::update: *(*(*(toc+0x25f4))+0xcc)+0xbc ; one call = one rendered frame
        const uint32_t frameDtAddr = c.rd32(c.rd32(c.rd32(0xc6f210 + 0x25f4)) + 0xcc) + 0xbc;
        const float stepDt = c.rdf32(W + 0x9c);
        auto frame = [&](int substeps) { c.wrf32(frameDtAddr, stepDt * float(substeps)); g.prepare(c); c.r[3] = W; c.f[1] = stepDt * substeps; rc_call(c, 0x31bab8); steps += substeps; };
        auto step1 = [&] { frame(1); };
        auto inEq = [&](const uint8_t* a2, const uint8_t* b2) { return std::memcmp(a2 + 0x410, b2 + 0x410, 0x18) == 0; };
        while (j + 1 < R && steps < 5000) {
            // next sample whose clock (+0x374, one tick per physics step) moved on
            size_t nx = j + 1;
            while (nx < R && bf(rec(nx), 0x374) == bf(rec(j), 0x374)) ++nx;
            if (nx >= R) break;
            const uint8_t* in = rec(nx);
            const int n = std::clamp(int(std::lround((bf(in, 0x374) - bf(rec(j), 0x374)) * 60.0)), 1, 30);
            // input schedules: the change between the two samples happened before step k (k = 1..n)
            size_t hit = SIZE_MAX;
            const bool change = !inEq(rec(j), in);
            if (change || n >= 2) save();
            const int stepsBefore = steps;
            // variants: when the input changed (before step k), and for gaps of 2+ steps whether they were one long frame
            const int k0 = change ? 1 : 0, k1 = change ? n : 0;
            for (int variant = 0; variant < 2 && hit == SIZE_MAX; ++variant) {
            if (variant == 1 && n < 2) break;
            if (variant == 1) { restore(); steps = stepsBefore; }
            for (int k = k0; k <= k1; ++k) {
                if (k > k0 || variant == 1) { restore(); steps = stepsBefore; }
                if (variant == 0) { for (int s2 = 0; s2 < n; ++s2) { setIn(s2 < k ? rec(j) : in); step1(); } }
                else { setIn(k > 0 ? rec(j) : in); frame(n); }
                if (same(in)) { hit = nx; break; }
                if (variant == 1) break;
            }
            }
            if (c.missing || c.unimplAt) { std::printf("missing 0x%x unimpl 0x%x\n", c.missing, c.unimplAt); break; }
            if (hit != SIZE_MAX) {
                ++exact; j = hit;
                if (std::getenv("FULLDIFF")) {
                    const uint8_t* e2 = rec(hit);
                    int nd = 0;
                    for (uint32_t o = 0; o < 0x1000; o += 4)
                        if (std::memcmp(e2 + o, c.mem + car + o, 4) != 0 && !(o >= 0x270 && o < 0x290) && o != 0x5c4 && o != 0x5cc) ++nd;
                    if (nd) std::printf("   rec %zu: %d other words differ\n", hit, nd);
                }
                continue;
            }
            // a sample taken in the middle of a step, or a real divergence
            double e = 0;
            for (int a2 = 0; a2 < 3; ++a2) e = std::max(e, double(std::fabs(bf(in, 0x140 + 4 * a2) - c.rdf32(car + 0x140 + 4 * a2))));
            worst = std::max(worst, e);
            if (firstBad < 0) {
                firstBad = steps;
                double t; std::memcpy(&t, rec(nx) - 8, 8);
                std::printf("first mismatch at step %d (record %zu, t=%.3f, %d steps): vel port %.5f %.5f %.5f emu %.5f %.5f %.5f\n", steps, nx, t, n,
                            c.rdf32(car + 0x140), c.rdf32(car + 0x144), c.rdf32(car + 0x148), bf(in, 0x140), bf(in, 0x144), bf(in, 0x148));
            }
            if (e > 1e-3) std::printf("step %d rec %zu vel err %.6f\n", steps, nx, e);
            if (e > 5.0) break;
            j = nx;
        }
        std::printf("steps %d, bit-exact %d, first mismatch %d, worst vel error %.6f m/s\n", steps, exact, firstBad, worst);
        return 0;
    }
    if (argc > 6 && std::string(argv[4]) == "world") {   // world <W> <fn> [N] [dt]: run fn(W, dt) N times
        const uint32_t W = uint32_t(std::strtoul(argv[5], nullptr, 16)), fn = uint32_t(std::strtoul(argv[6], nullptr, 16));
        const int N = argc > 7 ? std::atoi(argv[7]) : 10;
        const double dt = argc > 8 ? std::atof(argv[8]) : 1.0 / 60.0;
        FILE* nf = std::fopen((std::string(argv[2]) + ".next").c_str(), "rb");
        std::vector<uint8_t> nx;
        if (nf) { uint8_t buf[65536]; size_t k; while ((k = std::fread(buf, 1, sizeof buf, nf)) > 0) nx.insert(nx.end(), buf, buf + k); std::fclose(nf); }
        const size_t stride = 8 + 0x1000;
        const size_t frames = nx.size() > 16 ? (nx.size() - 16) / stride : 0;
        auto ef = [&](size_t i, uint32_t off) { const uint8_t* b = nx.data() + 16 + i * stride + 8 + off; uint32_t u = uint32_t(b[0]) << 24 | b[1] << 16 | b[2] << 8 | b[3]; float f; std::memcpy(&f, &u, 4); return f; };
        // emulator physics steps = records where +0xf0 (predicted position) changed
        std::vector<size_t> steps;
        for (size_t i = 1; i < frames; ++i) if (ef(i, 0xf8) != ef(i - 1, 0xf8)) steps.push_back(i);
        PPU c;
        g_watchLo = std::getenv("WATCH") ? car + uint32_t(std::strtoul(std::getenv("WATCH"), nullptr, 16)) : 0;
        g_watchHi = g_watchLo ? g_watchLo + 4 : 0;
        for (int it = 0; it < N; ++it) {
            const float a0 = c.mem ? c.rdf32(W + 0xa0) : 0;
            if (const char* in = std::getenv("INPUT")) {   // thr,brake,steer,handbrake raw player inputs
                float v[4] = {0, 0, 0, 0};
                std::sscanf(in, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
                g.prepare(c);
                c.wrf32(car + 0x410, v[0]); c.wrf32(car + 0x418, v[1]); c.wrf32(car + 0x420, v[2]); c.wrf32(car + 0x41c, v[3]);
            }
            if (const char* ef2 = std::getenv("ENGFN2")) {   // extra per-frame call on the engine object (car+0xb4)
                g.prepare(c);
                unsigned efn = 0, eoff = 0xb4;
                std::sscanf(ef2, "%x,%x", &efn, &eoff);
                c.r[3] = eoff ? c.rd32(car + eoff) : car; c.f[1] = dt; c.r[4] = 0;
                rc_call(c, efn);
                if (c.missing || c.unimplAt) std::printf("   engine fn: missing 0x%x unimpl 0x%x\n", c.missing, c.unimplAt);
                c.missing = c.unimplAt = 0;
            }
            g.prepare(c); c.r[3] = W; c.f[1] = dt;
            rc_call(c, fn);
            if (std::getenv("INPUT")) {
                const uint32_t eng = c.rd32(car + 0xb4);
                std::printf("   in %.2f %.2f %.2f %.2f smooth thr %.3f brk %.3f steer %.3f  rpm %.0f gear %d kmh %.1f\n", c.rdf32(car + 0x410), c.rdf32(car + 0x418), c.rdf32(car + 0x420), c.rdf32(car + 0x41c),
                            c.rdf32(car + 0x3f4), c.rdf32(car + 0x3f8), c.rdf32(car + 0x404), c.rdf32(eng + 0x1ac), int(c.rd32(eng + 0x1a4)),
                            3.6f * std::sqrt(c.rdf32(car + 0x140) * c.rdf32(car + 0x140) + c.rdf32(car + 0x148) * c.rdf32(car + 0x148)));
            }
            if (std::getenv("SHADOW")) {
                const uint32_t lp = c.rd32(W + 0xdc), n = c.rd32(W + 0xe0), cap = c.rd32(W + 0xe4);
                std::printf("   active list 0x%x n %u cap %u :", lp, n, cap);
                for (uint32_t k = 0; k < n && k < 12; ++k) std::printf(" %x", c.rd32(lp + 4 * k));
                std::printf("\n");
            }
            if (const char* wa = std::getenv("WATCHV3")) {   // WATCHV3=addr,addr,...: print a float triple at each address
                std::string sv = wa;
                size_t p0 = 0;
                std::printf("   v3:");
                while (p0 < sv.size()) {
                    const size_t p1 = sv.find(',', p0);
                    const uint32_t a = uint32_t(std::strtoul(sv.substr(p0, p1 - p0).c_str(), nullptr, 16));
                    std::printf(" [%.3f %.3f %.3f]", c.rdf32(a), c.rdf32(a + 4), c.rdf32(a + 8));
                    if (p1 == std::string::npos) break;
                    p0 = p1 + 1;
                }
                std::printf("\n");
            }
            if (const char* raw = std::getenv("RAWDUMP")) {   // RAWDUMP=file: every frame, car + 4 wheel objects as raw words
                static FILE* rf = std::fopen(raw, "wb");
                if (rf) {
                    std::fwrite(c.mem + car, 1, 0x1000, rf);
                    for (int wi = 0; wi < 4; ++wi) std::fwrite(c.mem + c.rd32(car + 0x2c8 + 4 * wi), 1, 0x400, rf);
                    std::fflush(rf);
                }
            }
            if (const char* dumpObj = std::getenv("DUMPRANGE")) {   // DUMPRANGE=lo,hi (car offsets): floats in 500..20000 or small ints
                unsigned lo = 0, hi = 0, ptr = 0;
                std::sscanf(dumpObj, "%x,%x,%x", &lo, &hi, &ptr);
                const uint32_t car0 = car;
                const uint32_t car = ptr ? c.rd32(car0 + ptr) : car0;
                std::printf("   dump:");
                for (uint32_t o = lo; o < hi; o += 4) {
                    const float v = c.rdf32(car + o);
                    const uint32_t u = c.rd32(car + o);
                    if ((std::fabs(v) > 500 && std::fabs(v) < 20000) || (u > 0 && u < 8)) std::printf(" +%x=%g", o, (u > 0 && u < 8) ? double(u) : double(v));
                }
                std::printf("\n");
            }
            std::printf("call %2d acc %.5f->%.5f  pred %9.4f %9.4f %9.4f vel %8.4f %8.4f %8.4f av %7.4f %s%s\n", it, a0, c.rdf32(W + 0xa0),
                        c.rdf32(car + 0xf0), c.rdf32(car + 0xf4), c.rdf32(car + 0xf8), c.rdf32(car + 0x140), c.rdf32(car + 0x144), c.rdf32(car + 0x148), c.rdf32(car + 0x150),
                        c.missing ? "MISSING " : "", c.unimplAt ? "UNIMPL" : "");
            if (c.missing) std::printf("   missing 0x%x\n", c.missing);
            if (c.unimplAt) std::printf("   unimpl 0x%x\n", c.unimplAt);
            if (c.missing || c.unimplAt) break;
            size_t e = SIZE_MAX;   // emulator record holding exactly this predicted position
            for (size_t i = 0; i < frames; ++i) {
                const uint8_t* b = nx.data() + 16 + i * stride + 8 + 0xf0;
                if (std::memcmp(b, c.mem + car + 0xf0, 12) == 0) e = i;   // last such record (final state of the step)
            }
            if (e != SIZE_MAX) {
                std::printf("emu  %2zu            pred %9.4f %9.4f %9.4f vel %8.4f %8.4f %8.4f av %7.4f\n", e, ef(e, 0xf0), ef(e, 0xf4), ef(e, 0xf8), ef(e, 0x140), ef(e, 0x144), ef(e, 0x148), ef(e, 0x150));
                // bit-exact check of the car object (the emulator sample may land mid-frame: only count words of the physics state)
                const uint8_t* eb = nx.data() + 16 + e * stride + 8;
                int nd = 0, shown = 0;
                for (uint32_t o = 0; o < 0x1000; o += 4)
                    if (std::memcmp(eb + o, c.mem + car + o, 4) != 0) {
                        ++nd;
                        if (std::getenv("SHOWDIFF") && shown++ < 40) {
                            uint32_t ue = uint32_t(eb[o]) << 24 | eb[o + 1] << 16 | eb[o + 2] << 8 | eb[o + 3]; float fe; std::memcpy(&fe, &ue, 4);
                            std::printf("     +%03x port %08x %12.6g emu %08x %12.6g\n", o, c.rd32(car + o), c.rdf32(car + o), ue, fe);
                        }
                    }
                std::printf("     words differing in car object: %d\n", nd);
            }
        }
        return 0;
    }
    if (argc > 6 && std::string(argv[4]) == "scan") {   // scan lo hi [before]: which function writes the history/orientation
        const uint32_t lo = uint32_t(std::strtoul(argv[5], nullptr, 16)), hi = uint32_t(std::strtoul(argv[6], nullptr, 16));
        const bool before = argc > 7;
        for (size_t k = 0; k < kFuncCount; ++k) {
            const uint32_t fn = kFuncs[k].addr;
            if (fn < lo || fn >= hi) continue;
            std::memcpy(g.mem, pristine.data(), keep);
            std::memcpy(g.mem + 0xd0000000, stacks.data(), stacks.size());
            PPU c;
            auto call = [&](uint32_t f, uint32_t r3) { g.prepare(c); c.r[3] = r3; c.r[4] = 0; c.r[5] = 0; c.f[1] = 1.0 / 60.0; rc_call(c, f); };
            call(0x3367c0, car); call(0x313a70, car);
            if (before) { call(fn, car); call(0x332808, car); } else { call(0x332808, car); call(fn, car); }
            const float h = c.rdf32(car + 0x188), r = c.rdf32(car + 0x34), o = c.rdf32(car + 0x54), q = c.rdf32(car + 0x4f4);
            const bool hit = std::fabs(h + 195.476f) < 0.001f || std::fabs(r - 0.946328f) < 1e-5f || std::fabs(o - 85.2663f) < 1e-3f || std::fabs(q - 85.2005f) < 1e-3f;
            if (hit) std::printf("0x%x: 188 %.5f rot34 %.6f 54 %.5f 4f4 %.5f %s\n", fn, h, r, o, q, c.missing ? "missing" : "");
            std::fflush(stdout);
        }
        return 0;
    }
    if (argc > 5 && std::string(argv[4]) == "chain") {   // chain <emu_frame> fn... : call in order, diff whole car vs emu frame
        FILE* nf = std::fopen((std::string(argv[2]) + ".next").c_str(), "rb");
        std::vector<uint8_t> nx;
        if (nf) { uint8_t buf[65536]; size_t k; while ((k = std::fread(buf, 1, sizeof buf, nf)) > 0) nx.insert(nx.end(), buf, buf + k); std::fclose(nf); }
        const size_t stride = 8 + 0x1000;
        const int ef = std::atoi(argv[5]);
        PPU c;
        for (int ai = 6; ai < argc; ++ai) {
            const uint32_t fn = uint32_t(std::strtoul(argv[ai], nullptr, 16));
            g.prepare(c); c.r[3] = car; c.r[4] = 0; c.r[5] = 0; c.f[1] = 1.0 / 60.0;
            rc_call(c, fn);
            if (c.missing || c.unimplAt) std::printf("0x%x: missing 0x%x unimpl 0x%x\n", fn, c.missing, c.unimplAt);
        }
        const uint8_t* e = nx.data() + 16 + ef * stride + 8;
        int nd = 0;
        for (uint32_t o = 0; o < 0x1000; o += 4)
            if (std::memcmp(e + o, c.mem + car + o, 4) != 0) {
                uint32_t ue = uint32_t(e[o]) << 24 | e[o + 1] << 16 | e[o + 2] << 8 | e[o + 3]; float fe; std::memcpy(&fe, &ue, 4);
                uint32_t sv = c.rd32(car + o);
                if (nd++ < 120) std::printf("   diff +%03x port %14.6g emu %14.6g  (%08x / %08x)\n", o, c.rdf32(car + o), fe, sv, ue);
            }
        std::printf("differing words vs emu frame %d: %d\n", ef, nd);
        return 0;
    }
    if (argc > 4) {   // explicit list of functions to call with r3 = car, f1 = dt (after running the car update first)
        for (int ai = 4; ai < argc; ++ai) {
            const uint32_t fn = uint32_t(std::strtoul(argv[ai], nullptr, 16));
            for (int pre = 0; pre < 2; ++pre) {
                std::memcpy(g.mem, pristine.data(), keep);
                std::memcpy(g.mem + 0xd0000000, stacks.data(), stacks.size());
                PPU c;
                g.prepare(c);
                if (pre) { c.r[3] = car; c.f[1] = 1.0 / 60.0; rc_call(c, 0x3367c0); g.prepare(c); c.r[3] = car; c.f[1] = 1.0 / 60.0; rc_call(c, 0x313a70); g.prepare(c); }
                c.r[3] = car; c.r[4] = car; c.r[5] = 0; c.f[1] = 1.0 / 60.0;
                rc_call(c, fn);
                const auto s1 = st(c);
                std::printf("%s0x%x: pos %.4f %.4f %.4f vel %.4f %.4f %.4f angvel %.4f %.4f %.4f %s%s\n", pre ? "update+" : "", fn,
                            s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6], s1[7], s1[8], c.missing ? "missing " : "", c.unimplAt ? "unimpl" : "");
            }
        }
        return 0;
    }
    for (auto& o : objs) {
        const uint32_t vt = c0.rd32(o.addr);
        std::printf("== %s 0x%x vtable 0x%x\n", o.name, o.addr, vt);
        for (int k = 0; k < 48; ++k) {
            const uint32_t opd = c0.rd32(vt + 4 * k);
            if (opd < 0x10000 || opd > 0x2000000) break;
            const uint32_t fn = c0.rd32(opd), toc = c0.rd32(opd + 4);
            if (fn < 0x10000 || fn > 0xbd0000) break;
            std::memcpy(g.mem, pristine.data(), keep);
            std::memcpy(g.mem + 0xd0000000, stacks.data(), stacks.size());
            PPU c;
            g.prepare(c);
            c.r[2] = toc;
            c.r[3] = o.addr;
            c.r[4] = 0; c.r[5] = 0;
            c.f[1] = 1.0 / 60.0;
            rc_call(c, fn);
            const auto s1 = st(c);
            bool changed = false;
            for (int i = 0; i < 13; ++i) if (s1[i] != s0[i]) changed = true;
            std::printf("  [%2d] 0x%x %s%s%s\n", k, fn, changed ? "CHANGES STATE " : "", c.missing ? "missing " : "", c.unimplAt ? "unimpl" : "");
            if (changed)
                std::printf("       -> pos %.4f %.4f %.4f vel %.4f %.4f %.4f angvel %.4f %.4f %.4f rot34 %.5f f8 %.5f 188 %.5f 54 %.5f\n", s1[0], s1[1], s1[2], s1[3], s1[4], s1[5], s1[6], s1[7], s1[8], s1[9], s1[10], s1[11], s1[12]);
        }
    }
    return 0;
}
