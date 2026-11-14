// RIPPROF=<seconds>: sampling profiler for one host thread: the one whose name contains RIPTHREAD (default "fifo",
// the RSX FIFO thread; guest threads register with their PS3 name, e.g. "main" or "Render"). Starting that many
// seconds after launch it records the thread's instruction pointer every 250 us for RIPLEN seconds (default 15) and writes one line
// per sample to ripprof.txt: the offset from the image base for code of the exe (resolve with
// addr2line -f -e recomp_boot.exe, base 0x140000000), "@module+offset" for anything else (GL driver, system).
#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
namespace gti::recomp {
// RIPTHREAD may list several names separated by commas ("main,Render,fifo,spu"): every matching thread is sampled in
// the same pass and written to ripprof_<name>.txt (the first one also to ripprof.txt).
struct Target { HANDLE h; char name[32]; };
static Target g_targets[24];
static std::atomic<int> g_nTargets{0};
static std::atomic<bool> g_samplerOn{false};
static const auto g_launch = std::chrono::steady_clock::now();
static double g_sinceLaunch() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_launch).count(); }
static void sampler(double startSec) {
    std::this_thread::sleep_for(std::chrono::microseconds(int64_t(startSec * 1e6)));
    const uint64_t base = uint64_t(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    const int n = g_nTargets.load();
    std::vector<std::vector<uint64_t>> rips(n);
    std::vector<float> times;   // RIPTIME=1: seconds since launch of every sample of the first thread, in ripprof_t.txt
    const double len = std::getenv("RIPLEN") ? std::atof(std::getenv("RIPLEN")) : 15.0;
    const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(int64_t(len * 1e6));
    while (std::chrono::steady_clock::now() < end) {
        for (int i = 0; i < n; ++i) {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (SuspendThread(g_targets[i].h) != DWORD(-1)) {
                if (GetThreadContext(g_targets[i].h, &ctx)) { rips[i].push_back(ctx.Rip); if (!i) times.push_back(float(g_sinceLaunch())); }
                ResumeThread(g_targets[i].h);
            }
        }
        const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(250);
        while (std::chrono::steady_clock::now() < until) YieldProcessor();
    }
    for (int i = 0; i < n; ++i) {
        char fn[64];
        std::snprintf(fn, sizeof fn, "ripprof_%s.txt", g_targets[i].name);
        for (int pass = 0; pass < (i ? 1 : 2); ++pass) {
            FILE* f = std::fopen(pass ? "ripprof.txt" : fn, "w");
            if (!f) continue;
            for (uint64_t r : rips[i]) {
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
    }
    if (std::getenv("RIPTIME")) if (FILE* t = std::fopen("ripprof_t.txt", "w")) { for (float x : times) std::fprintf(t, "%.4f\n", x); std::fclose(t); }
}
void ripprof_register_thread(const char* name) {
    const char* e = std::getenv("RIPPROF");
    if (!e || !name || g_samplerOn.load()) return;
    const char* list = std::getenv("RIPTHREAD") ? std::getenv("RIPTHREAD") : "fifo";
    bool match = false;
    for (const char* p = list; *p;) {
        const char* c = std::strchr(p, ',');
        const std::string want(p, c ? size_t(c - p) : std::strlen(p));
        if (!want.empty() && std::strstr(name, want.c_str())) match = true;
        if (!c) break;
        p = c + 1;
    }
    if (!match) return;
    static std::mutex mx;
    std::lock_guard<std::mutex> lk(mx);
    const int i = g_nTargets.load();
    if (i >= 24) return;
    g_targets[i].h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    if (!g_targets[i].h) return;
    std::snprintf(g_targets[i].name, sizeof g_targets[i].name, "%s", name);
    for (char* q = g_targets[i].name; *q; ++q) if (!std::isalnum(uint8_t(*q))) *q = '_';
    g_nTargets = i + 1;
    // the sampler starts once, when the first thread registers; threads registering later (before the start time) join in
    if (i == 0) std::thread([e] {
        const double at = std::max(0.0, std::atof(e) - g_sinceLaunch());
        std::this_thread::sleep_for(std::chrono::microseconds(int64_t(at * 1e6)));
        g_samplerOn = true;
        sampler(0);
    }).detach();
}
} // namespace gti::recomp
#else
namespace gti::recomp { void ripprof_register_thread(const char*) {} }
#endif
