// Runs the original per-frame game function (0x41d318) from a RPCS3 memory snapshot with all game code recompiled,
// watching writes to a car's velocity/position to find which original functions commit the physics step.
// usage: recomp_frame EBOOT.elf snapshot car_hex [arg_hex]
#include "recomp/guest.h"
#include "recomp/hle.h"
#include <cstdio>
#include <cstdlib>
#include <map>

using namespace gti::recomp;
namespace gti::recomp { void f_0041d318(PPU& c); }

static std::map<uint64_t, int> g_sysc;
static void sys_stub(PPU& c) {
    g_sysc[c.r[11]]++;
    if (c.r[11] == 403) {   // sys_tty_write(ch, buf, len, *written)
        const uint32_t buf = uint32_t(c.r[4]), len = uint32_t(c.r[5]);
        std::fwrite(c.mem + buf, 1, len, stdout);
        if (c.r[6]) c.wr32(uint32_t(c.r[6]), len);
    }
    if (c.r[11] == 348) {   // sys_memory_allocate(size, flags, u32* addr): hand out fresh guest memory
        static uint32_t next = 0x70000000;
        const uint32_t size = uint32_t(c.r[3]);
        const uint32_t align = (c.r[4] & 0x400) ? 0x100000 : 0x10000;   // 1 MB or 64 KB pages
        next = (next + align - 1) & ~(align - 1);
        c.wr32(uint32_t(c.r[5]), next);
        next += size;
    }
    if (c.r[11] == 352) {   // sys_memory_get_user_memory_size(info*): {u32 total, u32 available}
        c.wr32(uint32_t(c.r[3]), 0x0D000000);
        c.wr32(uint32_t(c.r[3]) + 4, 0x04000000);
    }
    c.r[3] = 0;
}

int main(int argc, char** argv) {
    if (argc < 4) { std::printf("usage: recomp_frame EBOOT.elf snapshot car_hex [arg_hex]\n"); return 1; }
    Guest g;
    g.stackTop = 0xEF000000;   // away from the snapshot's own thread stacks
    if (!g.init(argv[1]) || !g.loadSnapshot(argv[2])) { std::printf("load failed\n"); return 1; }
    const uint32_t car = uint32_t(std::strtoul(argv[3], nullptr, 16));
    hle_install();
    g_syscallHook = sys_stub;
    static HostHook hleHook = g_hostHook;
    g_hostHook = [](PPU& c, uint32_t addr) -> bool {
        if (addr == 0xa7d4b8 || addr == 0x48f778 || addr == 0x489958 || addr == 0x4896b0)
            std::printf("[trace] call 0x%x  r2=0x%llx r3=0x%llx lr=0x%llx\n", addr, (unsigned long long)c.r[2], (unsigned long long)c.r[3], (unsigned long long)c.lr);
        return hleHook(c, addr);
    };
    PPU c;
    g.prepare(c);
    // argument of 0x41d318: the caller's r30, saved by 0x41d318's prologue at frame+0x110 (frame = 0xd0100880 in snap2)
    const uint32_t arg = argc >= 5 ? uint32_t(std::strtoul(argv[4], nullptr, 16)) : uint32_t(c.rd64(0xd0100990));
    std::printf("frame object 0x%x\n", arg);
    std::printf("car before: pos %.4f %.4f %.4f vel %.4f %.4f %.4f\n", c.rdf32(car + 0x90), c.rdf32(car + 0x94), c.rdf32(car + 0x98),
                c.rdf32(car + 0x140), c.rdf32(car + 0x144), c.rdf32(car + 0x148));
    g_watchLo = car + 0x90;
    g_watchHi = car + 0x160;
    c.r[3] = arg;
    f_0041d318(c);
    g_watchLo = g_watchHi = 0;
    std::printf("car after : pos %.4f %.4f %.4f vel %.4f %.4f %.4f\n", c.rdf32(car + 0x90), c.rdf32(car + 0x94), c.rdf32(car + 0x98),
                c.rdf32(car + 0x140), c.rdf32(car + 0x144), c.rdf32(car + 0x148));
    if (c.missing) std::printf("MISSING call target 0x%x\n", c.missing);
    if (c.unimplAt) std::printf("UNIMPLEMENTED at 0x%x\n", c.unimplAt);
    for (auto& kv : g_sysc) std::printf("syscall %llu x%d\n", (unsigned long long)kv.first, kv.second);
    return 0;
}
