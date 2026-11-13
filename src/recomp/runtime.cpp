#include "recomp/ppu.h"
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <vector>
#include <mutex>
#include <thread>

namespace gti::recomp {

HostHook g_hostHook = nullptr;
uint64_t g_missLr = 0;

uint64_t g_timebaseFixed = 0;   // >0: deterministic time base, advances this many ticks per read
uint64_t rc_timebase() {
    if (g_timebaseFixed) { static uint64_t t = 1; t += g_timebaseFixed; return t; }
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    const double s = duration<double>(steady_clock::now() - t0).count();
    return uint64_t(s * 79800000.0) + 1;
}
uint32_t g_watchLo = 0, g_watchHi = 0;
// current chain of recompiled functions, per host thread (watch reports, crash reports, hang diagnosis)
struct ShadowReg { std::mutex m; std::vector<std::pair<std::thread::id, std::vector<uint32_t>*>> all; };
static ShadowReg& shadow_reg() { static ShadowReg r; return r; }
static std::vector<uint32_t>& shadow_vec() {
    thread_local std::vector<uint32_t>* v = nullptr;
    if (!v) { v = new std::vector<uint32_t>(); v->reserve(256); std::lock_guard<std::mutex> lk(shadow_reg().m); shadow_reg().all.push_back({std::this_thread::get_id(), v}); }
    return *v;
}
#define g_shadow shadow_vec()
void rc_dump_all_shadows() {   // racy snapshot of every thread's call chain (debug only)
    std::lock_guard<std::mutex> lk(shadow_reg().m);
    for (auto& t : shadow_reg().all) {
        std::vector<uint32_t> v = *t.second;
        std::printf("[thread chain] depth %zu:", v.size());
        for (size_t i = v.size() > 24 ? v.size() - 24 : 0; i < v.size(); ++i) std::printf(" %x", v[i]);
        std::printf("\n");
    }
    std::fflush(stdout);
}
bool g_shadowOn = false;
bool g_traceCalls = false;   // print every recompiled function entered (debug)                 // keep the chain also without a watch (crash reports)
void rc_dump_shadow() {
    std::printf("[chain] depth %zu:", g_shadow.size());
    const size_t n = g_shadow.size();
    for (size_t i = n > 40 ? n - 40 : 0; i < n; ++i) std::printf(" %x", g_shadow[i]);
    std::printf("\n");
    std::fflush(stdout);
}

void watch_hit(PPU& c, uint32_t addr, int size) {
    std::printf("[watch] write %d bytes at 0x%x  lr=0x%llx  chain:", size, addr, (unsigned long long)c.lr);
    const size_t n = g_shadow.size();
    for (size_t i = n > 12 ? n - 12 : 0; i < n; ++i) std::printf(" %x", g_shadow[i]);
    std::printf("\n");
}
SyscallHook g_syscallHook = nullptr;

void rc_syscall(PPU& c) {
    if (g_syscallHook) { g_syscallHook(c); return; }
    c.unimpl(0xFFFF0000u | uint32_t(c.r[11]));   // marks which syscall number was needed
}

// Fast dispatch: a flat table from code address to recompiled function, built on the first call. Every call of the
// game goes through rc_call (tens of millions per second), and the map lookup of the host hook plus a binary search
// over the ~30000 functions were a tenth of the main thread. Addresses with a host hook keep the slow path.
static std::vector<uint32_t>& hooked_list() { static std::vector<uint32_t> v; return v; }
void rc_mark_hooked(uint32_t addr) { hooked_list().push_back(addr); }
namespace {
struct FastTable {
    uint32_t lo = 0, hi = 0;
    std::vector<Fn> fn;   // nullptr: not a function start, or hooked
    FastTable() {
        if (!kFuncCount) return;
        lo = kFuncs[0].addr; hi = kFuncs[kFuncCount - 1].addr + 4;
        for (size_t k = 0; k < kFuncCount; ++k) { lo = std::min(lo, kFuncs[k].addr); hi = std::max(hi, kFuncs[k].addr + 4); }
        fn.assign((hi - lo) / 4, nullptr);
        for (size_t k = 0; k < kFuncCount; ++k) if (!(kFuncs[k].addr & 3)) fn[(kFuncs[k].addr - lo) / 4] = kFuncs[k].fn;
        for (uint32_t a : hooked_list()) if (a >= lo && a < hi) fn[(a - lo) / 4] = nullptr;
    }
};
}
void rc_call(PPU& c, uint32_t addr) {
    if (c.missing || c.unimplAt) return;          // stop executing once something went wrong
    static const FastTable ft;
    static const bool slowOnly = std::getenv("RCSLOWCALL") != nullptr;
    if (!slowOnly && addr - ft.lo < ft.hi - ft.lo && !g_shadowOn && !g_traceCalls && g_watchHi <= g_watchLo) {
        if (const Fn f = ft.fn[(addr - ft.lo) >> 2]) { f(c); return; }
    }
    if (g_hostHook && g_hostHook(c, addr)) return;
    const FuncEntry* b = kFuncs;
    const FuncEntry* e = kFuncs + kFuncCount;
    const FuncEntry* it = std::lower_bound(b, e, addr, [](const FuncEntry& f, uint32_t a) { return f.addr < a; });
    if (it != e && it->addr == addr) {
        if (g_traceCalls) std::printf("[fn] %x r3=%llx r4=%llx r5=%llx r6=%llx\n", addr, (unsigned long long)c.r[3], (unsigned long long)c.r[4], (unsigned long long)c.r[5], (unsigned long long)c.r[6]);
        const bool sh = g_shadowOn || g_watchHi > g_watchLo;
        if (sh) g_shadow.push_back(addr);
        it->fn(c);
        if (sh && !g_shadow.empty()) g_shadow.pop_back();
        return;
    }
    c.missing = addr;
    g_missLr = c.lr;   // where the untranslated target was called from
    if (g_shadowOn) { std::printf("[missing 0x%x] ", addr); rc_dump_shadow(); }
}

} // namespace gti::recomp
