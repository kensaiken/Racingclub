// Calls the game's own snprintf (0x457ac0, recompiled) with known arguments to validate the translation.
#include "recomp/guest.h"
#include "recomp/hle.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace gti::recomp;
namespace gti::recomp { extern bool g_traceCalls; extern uint32_t g_watchLo, g_watchHi; }
int main(int argc, char** argv) {
    static Guest g;
    if (!g.init(argv[1])) return 1;
    hle_install();
    static HostHook inner = g_hostHook;
    g_hostHook = [](PPU& c, uint32_t addr) -> bool {
        if (addr == 0x974248) {
            const uint32_t va = c.rd32(uint32_t(c.r[4]));
            std::printf("[%%s handler] mod '%c'(0x%02x) va=%x slot=%08x%08x r31=%llx prec@0x44=%d\n", c.rd8(uint32_t(c.r[3]) + 0x52), c.rd8(uint32_t(c.r[3]) + 0x52), va, c.rd32(va), c.rd32(va + 4), (unsigned long long)c.r[31], int32_t(c.rd32(uint32_t(c.r[31]) + 0x44)));
        }
        return inner(c, addr);
    };
    PPU c;
    g.prepare(c);
    c.r[13] = 0x60100000 + 0x7030;   // a TLS area
    const uint32_t buf = 0x60000000, fmt = 0x60001000, s1 = 0x60001100, s2 = 0x60001200, s3 = 0x60001300;
    std::strcpy((char*)g.mem + fmt, argc > 2 ? argv[2] : "Data/SumoTool/%s/%s/%s [%d] %x %5.2f");
    std::strcpy((char*)g.mem + s1, "ps3"); std::strcpy((char*)g.mem + s2, "accomplishments"); std::strcpy((char*)g.mem + s3, "accomplishments");
    c.r[3] = buf; c.r[4] = 0x200; c.r[5] = fmt; c.r[6] = s1; c.r[7] = s2; c.r[8] = s3; c.r[9] = 42; c.r[10] = 0xbeef;
    double d = 3.14159; uint64_t u; std::memcpy(&u, &d, 8); c.f[1] = d;
    c.wr64(uint32_t(c.r[1]) + 0x30 + 8 * 8, u);   // 9th argument slot (double) in the caller's parameter area
    g_traceCalls = std::getenv("TRACE") != nullptr;
    if (std::getenv("WATCHVA")) { g_watchLo = 0xd00fff48; g_watchHi = 0xd00fff78; }
    if (const char* wv = std::getenv("WATCHAT")) { g_watchLo = uint32_t(std::strtoul(wv, nullptr, 16)); g_watchHi = g_watchLo + 8; }
    rc_call(c, 0x457ac0);
    g_traceCalls = false;
    std::printf("returned %lld, missing 0x%x unimpl 0x%x\nresult: '%s'\n", (long long)c.r[3], c.missing, c.unimplAt, (char*)g.mem + buf);
}
