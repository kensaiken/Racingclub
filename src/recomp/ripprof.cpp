// RIPPROF=<seconds>: sampling profiler for one host thread: the one whose name contains RIPTHREAD (default "fifo",
// the RSX FIFO thread; guest threads register with their PS3 name, e.g. "main" or "Render"). Starting that many
// seconds after launch it records the thread's instruction pointer every 250 us for RIPLEN seconds (default 15) and writes one line
// per sample to ripprof.txt: the offset from the image base for code of the exe (resolve with
// addr2line -f -e recomp_boot.exe, base 0x140000000), "@module+offset" for anything else (GL driver, system).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
namespace gti::recomp {
static HANDLE g_target = nullptr;
static const auto g_launch = std::chrono::steady_clock::now();
static double g_sinceLaunch() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_launch).count(); }
static void sampler(double startSec) {
    std::this_thread::sleep_for(std::chrono::microseconds(int64_t(startSec * 1e6)));
    const uint64_t base = uint64_t(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    std::vector<uint64_t> rips;
    rips.reserve(70000);
    const double len = std::getenv("RIPLEN") ? std::atof(std::getenv("RIPLEN")) : 15.0;
    const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(int64_t(len * 1e6));
    while (std::chrono::steady_clock::now() < end) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (SuspendThread(g_target) != DWORD(-1)) {
            if (GetThreadContext(g_target, &ctx)) rips.push_back(ctx.Rip);
            ResumeThread(g_target);
        }
        const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(250);
        while (std::chrono::steady_clock::now() < until) YieldProcessor();
    }
    FILE* f = std::fopen("ripprof.txt", "w");
    if (!f) return;
    for (uint64_t r : rips) {
        if (r >= base && r - base < 0x10000000ull) { std::fprintf(f, "%llx\n", static_cast<unsigned long long>(r - base)); continue; }
        HMODULE h = nullptr;
        char name[260] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(uintptr_t(r)), &h) && h)
            GetModuleFileNameA(h, name, sizeof name);
        const char* slash = std::strrchr(name, '\\');
        std::fprintf(f, "@%s+%llx\n", slash ? slash + 1 : name, static_cast<unsigned long long>(r - uint64_t(reinterpret_cast<uintptr_t>(h))));
    }
    std::fclose(f);
}
void ripprof_register_thread(const char* name) {
    const char* e = std::getenv("RIPPROF");
    const char* want = std::getenv("RIPTHREAD") ? std::getenv("RIPTHREAD") : "fifo";
    if (!e || g_target || !name || !std::strstr(name, want)) return;
    g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    static const auto launch = std::chrono::steady_clock::now();
    (void)launch;
    if (g_target) std::thread(sampler, std::max(0.0, std::atof(e) - g_sinceLaunch())).detach();
}
} // namespace gti::recomp
#else
namespace gti::recomp { void ripprof_register_thread(const char*) {} }
#endif
