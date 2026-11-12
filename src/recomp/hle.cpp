// High-level emulation of the PS3 system libraries called by the recompiled game code.
// A call to an import stub address (see imports_table.inc) runs the host implementation here.
#include "recomp/hle.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <map>
#include <set>
#include <string>

namespace gti::recomp {

extern HleExt g_hleExt;

namespace {

struct Import { uint32_t addr; const char* module; const char* name; };
const Import kImports[] = {
#include "recomp/imports_table.inc"
};

std::set<std::string> g_seen;
const bool g_countOn = std::getenv("HLECOUNT") != nullptr;
std::mutex g_cntMx;
std::map<std::string, std::pair<uint64_t, uint32_t>> g_cnt;        // stubs that were hit (logged once)
uint32_t g_heapNext = 0x60000000;    // simple bump allocator for guest malloc (host-owned region)

uint32_t gmalloc(uint32_t size, uint32_t align = 16) {
    g_heapNext = (g_heapNext + align - 1) & ~(align - 1);
    const uint32_t a = g_heapNext;
    g_heapNext += std::max<uint32_t>(size, 1);
    return a;
}

void ret(PPU& c, uint64_t v) { c.r[3] = v; }

bool handle(PPU& c, const Import& im) {
    if (g_hleExt && g_hleExt(c, im.module, im.name)) return true;   // full-game layer (lv2.cpp) first
    const std::string n = im.name;
    uint8_t* m = c.mem;
    auto a32 = [&](int i) { return uint32_t(c.r[3 + i]); };
    if (n == "_sys_memcpy" || n == "_sys_memmove") { std::memmove(m + a32(0), m + a32(1), a32(2)); ret(c, a32(0)); return true; }
    if (n == "_sys_memset") { std::memset(m + a32(0), int(a32(1)), a32(2)); ret(c, a32(0)); return true; }
    if (n == "_sys_memcmp") { ret(c, uint64_t(int64_t(std::memcmp(m + a32(0), m + a32(1), a32(2))))); return true; }
    if (n == "_sys_strlen") { ret(c, std::strlen(reinterpret_cast<char*>(m + a32(0)))); return true; }
    if (n == "_sys_strcpy") { std::strcpy(reinterpret_cast<char*>(m + a32(0)), reinterpret_cast<char*>(m + a32(1))); ret(c, a32(0)); return true; }
    if (n == "_sys_strcmp") { ret(c, uint64_t(int64_t(std::strcmp(reinterpret_cast<char*>(m + a32(0)), reinterpret_cast<char*>(m + a32(1)))))); return true; }
    if (n == "_sys_strcat") { std::strcat(reinterpret_cast<char*>(m + a32(0)), reinterpret_cast<char*>(m + a32(1))); ret(c, a32(0)); return true; }
    if (n == "_sys_malloc") { ret(c, gmalloc(a32(0))); return true; }
    if (n == "_sys_memalign") { ret(c, gmalloc(a32(1), std::max<uint32_t>(a32(0), 16))); return true; }
    if (n == "_sys_free") { return true; }
    if (n == "_sys_printf" || n == "_sys_snprintf" || n == "_sys_sprintf") { ret(c, 0); return true; }
    // single host thread for now: the recompiled main thread
    constexpr uint32_t kTid = 0x01000100;
    if (n == "sys_ppu_thread_get_id") { c.wr64(a32(0), kTid); ret(c, 0); return true; }
    // sys_lwmutex_t: u64 lock_var {u32 owner, u32 waiter} @0, u32 attribute @8, u32 recursive_count @0xc, u32 sleep_queue @0x10
    if (n == "sys_lwmutex_lock" || n == "sys_lwmutex_trylock") {
        const uint32_t mtx = a32(0);
        const uint32_t owner = c.rd32(mtx);
        if (owner == kTid) c.wr32(mtx + 0xc, c.rd32(mtx + 0xc) + 1);
        else { c.wr32(mtx, kTid); c.wr32(mtx + 0xc, 1); }
        ret(c, 0); return true;
    }
    if (n == "sys_lwmutex_unlock") {
        const uint32_t mtx = a32(0);
        uint32_t cnt = c.rd32(mtx + 0xc);
        if (cnt > 1) c.wr32(mtx + 0xc, cnt - 1);
        else { c.wr32(mtx + 0xc, 0); c.wr32(mtx, 0xffffffffu); }
        ret(c, 0); return true;
    }
    if (n == "sys_lwmutex_create") { c.wr32(a32(0), 0xffffffffu); c.wr32(a32(0) + 4, 0); c.wr32(a32(0) + 0xc, 0); ret(c, 0); return true; }
    if (n == "sys_time_get_system_time") { ret(c, 1000000ull); return true; }
    // everything else: report once, answer CELL_OK
    if (g_seen.insert(std::string(im.module) + "::" + n).second)
        std::fprintf(stderr, "[hle] stub %s::%s\n", im.module, im.name);
    ret(c, 0);
    return true;
}

bool hook(PPU& c, uint32_t addr) {
    static std::map<uint32_t, const Import*> byAddr;
    if (byAddr.empty()) for (auto& im : kImports) byAddr[im.addr] = &im;
    auto it = byAddr.find(addr);
    if (it == byAddr.end()) return false;
    if (g_countOn) {   // HLECOUNT=1: count every import call (hle_dump_counts)
        std::lock_guard<std::mutex> lk(g_cntMx);
        auto& e = g_cnt[std::string(it->second->module) + "::" + it->second->name];
        e.first++; e.second = uint32_t(c.lr);
    }
    // the real import stub starts with `std r2, 0x28(r1)`; the caller reloads r2 from there after the call
    c.wr64(uint32_t(c.r[1]) + 0x28, c.r[2]);
    return handle(c, *it->second);
}

} // namespace

HleExt g_hleExt = nullptr;
void hle_dump_counts() {
    std::lock_guard<std::mutex> lk(g_cntMx);
    for (auto& [k, v] : g_cnt) std::fprintf(stderr, "[hlecount] %10llu  %s  (last lr %x)\n", (unsigned long long)v.first, k.c_str(), v.second);
}
void hle_install() { g_hostHook = hook; }

} // namespace gti::recomp
