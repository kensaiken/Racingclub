// Boots the whole recompiled game natively from its ELF entry point (no emulator, no snapshot).
// usage: recomp_boot <EBOOT.elf> <USRDIR> [--trace] [--seconds N]
#include "recomp/guest.h"
#include "recomp/lv2.h"
#include "recomp/hle.h"
#include "recomp/rsx_gl.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

using namespace gti::recomp;
namespace gti::recomp { extern bool g_shadowOn; void rc_dump_all_shadows(); extern uint32_t g_watchLo, g_watchHi; }

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: recomp_boot EBOOT.elf USRDIR [--trace] [--seconds N]\n"); return 1; }
    setvbuf(stdout, nullptr, _IONBF, 0);
    Lv2Config cfg;
    cfg.usrdir = argv[2];
    cfg.hdd0 = (std::filesystem::path(argv[0]).parent_path() / "dev_hdd0").string();
    int seconds = 20;
    for (int i = 3; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--trace")) cfg.trace = true;
        else if (!std::strcmp(argv[i], "--chain")) g_shadowOn = true;
        else if (!std::strcmp(argv[i], "--watch") && i + 1 < argc) { g_watchLo = uint32_t(std::strtoul(argv[++i], nullptr, 16)); g_watchHi = g_watchLo + 4; }
        else if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--pad") && i + 1 < argc) cfg.padScript = argv[++i];
    }
    std::filesystem::create_directories(cfg.hdd0);
    static Guest g;
    if (!g.init(argv[1])) { std::printf("cannot load %s\n", argv[1]); return 1; }
    lv2_install(g, cfg);
    // the main thread runs on its own host thread so this one can stop the test after a while
    std::thread mt([] { const int rc = lv2_run_main(g); std::fprintf(stderr, "[boot] main finished (%d)\n", rc); });
    mt.detach();
    if (std::getenv("SAMPLE")) {   // sample every thread's call chain twice a second
        for (int k = 0; k < seconds * 2; ++k) { std::this_thread::sleep_for(std::chrono::milliseconds(500)); rc_dump_all_shadows(); }
    } else if (seconds <= 0) {   // --seconds 0: play until the window is closed
        while (!rsx::quit_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::_Exit(0);
    } else
        for (int k = 0; k < seconds * 10 && !rsx::quit_requested(); ++k) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::fprintf(stderr, "[boot] %d s elapsed, stopping\n", seconds);
    if (g_shadowOn) rc_dump_all_shadows();
    hle_dump_counts();
    lv2_dump_waits();
    std::_Exit(0);
}
