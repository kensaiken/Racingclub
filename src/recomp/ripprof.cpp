// RIPPROF=<seconds>: sampling profiler for one host thread (the RSX FIFO thread registers itself). Starting that
// many seconds after launch it records the thread's instruction pointer every 250 us for 15 s and writes one line
// per sample to ripprof.txt: the offset from the image base for code of the exe (resolve with
// addr2line -f -e recomp_boot.exe, base 0x140000000), "@module+offset" for anything else (GL driver, system).
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
static void sampler(int startSec) {
    std::this_thread::sleep_for(std::chrono::seconds(startSec));
    const uint64_t base = uint64_t(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    std::vector<uint64_t> rips;
    rips.reserve(70000);
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
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
void ripprof_register_thread() {
    const char* e = std::getenv("RIPPROF");
    if (!e || g_target) return;
    g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    if (g_target) std::thread(sampler, std::atoi(e)).detach();
}
} // namespace gti::recomp
#else
namespace gti::recomp { void ripprof_register_thread() {} }
#endif
