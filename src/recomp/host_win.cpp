// Host display helpers for frame pacing. The game runs at 60 Hz and paces itself on the vblank; if the emulated
// vblank is a free-running timer and the monitor refreshes at something that is not a multiple of 60 (144 Hz),
// every image stays on screen for an irregular number of refreshes and 60 fps look choppy. These helpers let the
// vblank thread follow the real display: host_display_hz() / host_set_refresh() and host_wait_refresh().
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <cwchar>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
// kernel-mode-thunk vblank wait (gdi32): blocks until the next real vertical blank of the primary display
extern "C" {
struct KmtOpenAdapterFromHdc { HDC hDc; UINT hAdapter; LUID luid; UINT vidPnSourceId; };
struct KmtWaitVblank { UINT hAdapter; UINT hDevice; UINT vidPnSourceId; };
LONG __stdcall D3DKMTOpenAdapterFromHdc(KmtOpenAdapterFromHdc*);
LONG __stdcall D3DKMTWaitForVerticalBlankEvent(const KmtWaitVblank*);
}
namespace gti::recomp {
// refresh rate of the primary display in Hz (0 when unknown)
int host_display_hz() {
    DEVMODEW dm{};
    dm.dmSize = sizeof dm;
    return EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) ? int(dm.dmDisplayFrequency) : 0;
}
// Switches the primary display to `hz` at its current resolution (a dynamic change: nothing is saved, but it stays
// until host_restore_refresh() or the next mode change; the "temporary" CDS_FULLSCREEN flavour is accepted and then
// not applied by the compositor). False when the display has no such mode.
void host_restore_refresh();
static DEVMODEW g_savedMode{};
static bool g_modeChanged = false;
bool host_set_refresh(int hz) {
    DEVMODEW cur{};
    cur.dmSize = sizeof cur;
    if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &cur)) return false;
    if (int(cur.dmDisplayFrequency) == hz) return true;
    DEVMODEW dm{};
    dm.dmSize = sizeof dm;
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); ++i) {
        if (dm.dmPelsWidth != cur.dmPelsWidth || dm.dmPelsHeight != cur.dmPelsHeight || dm.dmBitsPerPel != cur.dmBitsPerPel || int(dm.dmDisplayFrequency) != hz) continue;
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
        if (ChangeDisplaySettingsExW(nullptr, &dm, nullptr, 0, nullptr) != DISP_CHANGE_SUCCESSFUL) return false;
        g_savedMode = cur;
        g_savedMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
        g_modeChanged = true;
        // the display must go back to its mode however the process ends (crash, Ctrl+C, console closed)
        SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG { host_restore_refresh(); return EXCEPTION_CONTINUE_SEARCH; });
        SetConsoleCtrlHandler([](DWORD) -> BOOL { host_restore_refresh(); return FALSE; }, TRUE);
        std::atexit([] { host_restore_refresh(); });
        return true;
    }
    return false;
}
// puts the display back as host_set_refresh() found it
void host_restore_refresh() {
    if (!g_modeChanged) return;
    g_modeChanged = false;
    ChangeDisplaySettingsExW(nullptr, &g_savedMode, nullptr, 0, nullptr);
}
// blocks until the next vertical blank of the primary display; false when that is not available
bool host_wait_refresh() {
    static KmtWaitVblank w{};
    static int state = 0;   // 0 not opened, 1 ok, -1 unavailable
    if (state == 0) {
        // a DC of the primary monitor itself: the screen DC (GetDC(nullptr)) spans every monitor, and with two of
        // them the wait did not follow the one the game is on (frames took two refreshes or more)
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof mi;
        const POINT origin{0, 0};
        GetMonitorInfoW(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &mi);
        KmtOpenAdapterFromHdc o{};
        o.hDc = CreateDCW(mi.szDevice, mi.szDevice, nullptr, nullptr);
        state = o.hDc && D3DKMTOpenAdapterFromHdc(&o) == 0 ? 1 : -1;
        if (o.hDc) DeleteDC(o.hDc);
        w.hAdapter = o.hAdapter; w.vidPnSourceId = o.vidPnSourceId;
    }
    if (state < 0) return false;
    if (D3DKMTWaitForVerticalBlankEvent(&w) != 0) { state = -1; return false; }
    return true;
}
// 1 ms Windows timer for the whole run. Short sleeps (the SPU job workers, the RSX thread and the vblank pacing
// all sleep tens of microseconds to a few milliseconds) otherwise last a whole 15.6 ms tick: on Windows 11 the
// resolution another program asked for no longer applies to this one, and frames halved to 30 per second.
void host_timer_init() {
    static const bool done = [] {
        HMODULE w = LoadLibraryW(L"winmm.dll");
        auto begin = w ? reinterpret_cast<UINT(WINAPI*)(UINT)>(reinterpret_cast<void*>(GetProcAddress(w, "timeBeginPeriod"))) : nullptr;
        // and no "efficiency mode" when the window is in the background (alt-tab to OBS): Windows would slow
        // the threads down (EcoQoS) and the game would stutter
        PROCESS_POWER_THROTTLING_STATE pt{};
        pt.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
        pt.StateMask = 0;
        using SetInfo = BOOL(WINAPI*)(HANDLE, int, void*, DWORD);   // Windows 8+, looked up so older SDK headers do
        if (auto set = reinterpret_cast<SetInfo>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation"))))
            set(GetCurrentProcess(), int(ProcessPowerThrottling), &pt, sizeof pt);
        return begin && begin(1) == 0;
    }();
    (void)done;
    if (std::getenv("TIMERTEST")) {   // debug: how long short sleeps really take
        LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f);
        for (int us : {50, 500, 1000}) {
            QueryPerformanceCounter(&t0);
            for (int k = 0; k < 20; ++k) std::this_thread::sleep_for(std::chrono::microseconds(us));
            QueryPerformanceCounter(&t1);
            std::fprintf(stderr, "[timer] sleep %d us takes %.3f ms (timeBeginPeriod %s)%c", us, (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart / 20, done ? "ok" : "failed", 10);
        }
    }
}
// time-critical work (sound mixing): the calling thread goes ahead of the normal ones
// also puts the whole process above normal programs once, like most games: OBS or a browser then cannot take its time slices
// Sleeps of well under a millisecond: a high-resolution waitable timer (Windows 10 1803+) wakes up about 0.5 ms
// late at most, where Sleep() rounds up to the timer period. Threads of the game that poll with short sleeps then
// really sleep instead of burning a core.
void host_sleep_us(uint64_t us) {
    static thread_local HANDLE t = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    if (!t) { std::this_thread::sleep_for(std::chrono::microseconds(us)); return; }
    LARGE_INTEGER due; due.QuadPart = -int64_t(us) * 10;
    if (!SetWaitableTimer(t, &due, 0, nullptr, nullptr, FALSE)) { std::this_thread::sleep_for(std::chrono::microseconds(us)); return; }
    WaitForSingleObject(t, INFINITE);
}
void host_thread_priority_high() {
    if (std::getenv("NOPRIO")) return;
    static const bool cls = SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) != 0; (void)cls;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}
// The RSX thread is the one every image waits for. With a dozen busy threads on 8 cores Windows sometimes put it on
// the second hardware thread of a core a geometry worker was using: it then ran a third slower for seconds on end
// (the start grid stuck at ~95 images per second instead of 144, run after run at random). own = true keeps the
// calling thread on the first physical core (logical CPUs 0 and 1); own = false keeps it off that core. Only on
// machines with at least 12 logical CPUs. AFINIDAD=0: off.
void host_thread_core(bool own) {
    // Off unless AFINIDAD is set: measured, no placement beat the scheduler (the slow runs were the test window
    // losing the focus, not the cores).
    static const bool off = !std::getenv("AFINIDAD") || std::getenv("AFINIDAD")[0] == '0';
    DWORD_PTR proc = 0, sys = 0;
    if (off || !GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys)) return;
    int n = 0;
    for (DWORD_PTR m = proc; m; m >>= 1) n += int(m & 1);
    if (n < 12 || (proc & 3) != 3) return;
    // AFINIDAD=mode: 1 = own core is 0 (CPUs 0,1); 2 = own core is the last one; 3 = no pinning, only priority for the owner;
    // 4 = owner on one logical CPU of the last core, the others off that whole core
    static const int mode = std::getenv("AFINIDAD") ? std::atoi(std::getenv("AFINIDAD")) : 3;
    int hi = 0; for (int k = 0; k < 64; ++k) if (proc >> k & 1) hi = k;
    const DWORD_PTR last = DWORD_PTR(3) << (hi - 1);
    if (mode == 1) SetThreadAffinityMask(GetCurrentThread(), own ? DWORD_PTR(3) : (proc & ~DWORD_PTR(3)));
    else if (mode == 2) SetThreadAffinityMask(GetCurrentThread(), own ? last : (proc & ~last));
    else if (mode == 4) SetThreadAffinityMask(GetCurrentThread(), own ? (DWORD_PTR(1) << (hi - 1)) : (proc & ~last));
    if (own) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}
// Draws the options panel with GDI: n lines of "label<TAB>value" text (line 0 is the title, `sel` is highlighted,
// lines without a tab are notes) into w x h top-down BGRA pixels.
// "Aplicar cambios" in the options menu: starts the game again with the same command line and ends this process at
// once (the display mode goes back first, so the new process finds the desktop's own mode)
void host_restart() {
    host_restore_refresh();
    STARTUPINFOW si{}; si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = GetCommandLineW();
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    std::fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), 0);
}
// "Informe de problemas" in the options menu: writes informe_problema.txt next to the executable (processor, memory,
// graphics card, settings and the end of the log, with the Windows user name taken out of the paths), shows it in
// Explorer and opens the project's "new issue" page, where the user can attach it.
void host_report(const char* gpu) {
    std::fflush(nullptr);
    auto slurp = [](const char* name, size_t tail) {
        std::string s;
        if (FILE* f = std::fopen(name, "rb")) {
            std::fseek(f, 0, SEEK_END); const long n = std::ftell(f);
            const long from = n > long(tail) ? n - long(tail) : 0;
            std::fseek(f, from, SEEK_SET);
            s.resize(size_t(n - from)); s.resize(std::fread(s.data(), 1, s.size(), f)); std::fclose(f);
        }
        return s;
    };
    char cpu[128] = "?"; DWORD cb = sizeof cpu;
    RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", RRF_RT_REG_SZ, nullptr, cpu, &cb);
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
    SYSTEMTIME t; GetLocalTime(&t);
    char head[512];
    std::snprintf(head, sizeof head, "Racing Club+ - informe de problemas\r\nFecha: %04d-%02d-%02d %02d:%02d\r\nProcesador: %s (%u hilos)\r\nMemoria: %llu MB\r\nGrafica: %s\r\nPantalla: %d Hz\r\n\r\n--- gtiport.ini ---\r\n",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, cpu, std::thread::hardware_concurrency(), (unsigned long long)(ms.ullTotalPhys >> 20), gpu ? gpu : "?", host_display_hz());
    std::string out = head + slurp("gtiport.ini", 1 << 16) + "\r\n--- final de recomp_log.txt ---\r\n" + slurp("recomp_log.txt", 300000);
    char user[256]; DWORD un = sizeof user;
    if (GetUserNameA(user, &un) && un > 3) {   // paths in the log carry the Windows user name
        const std::string u = user;
        for (size_t p = 0; (p = out.find(u, p)) != std::string::npos;) out.replace(p, u.size(), "USUARIO");
    }
    wchar_t exe[1024]; std::wstring path = L"informe_problema.txt";
    if (const DWORD n = GetModuleFileNameW(nullptr, exe, 1024); n > 0 && n < 1024) { path.assign(exe, n); path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"informe_problema.txt"; }
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return;
    std::fwrite(out.data(), 1, out.size(), f); std::fclose(f);
    const std::wstring sel = L"/select,\"" + path + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", sel.c_str(), nullptr, SW_SHOWNORMAL);
    ShellExecuteW(nullptr, L"open", L"https://github.com/kensaiken/Racingclub/issues/new", nullptr, nullptr, SW_SHOWNORMAL);
}
bool host_menu_bitmap(const wchar_t* const* lines, int n, int sel, int w, int h, uint32_t* out) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bm = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (!bm) { if (dc) DeleteDC(dc); return false; }
    HGDIOBJ oldBm = SelectObject(dc, bm);
    const int lineH = h / (n + 1);
    RECT all{0, 0, w, h};
    HBRUSH bg = CreateSolidBrush(RGB(16, 20, 30)), hi = CreateSolidBrush(RGB(200, 40, 40)), bar = CreateSolidBrush(RGB(34, 42, 60));
    FillRect(dc, &all, bg);
    HFONT font = CreateFontW(-(lineH * 55 / 100), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    HFONT small = CreateFontW(-(lineH * 42 / 100), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    SetBkMode(dc, TRANSPARENT);
    for (int i = 0; i < n; ++i) {
        RECT r{0, lineH / 2 + i * lineH, w, lineH / 2 + (i + 1) * lineH};
        const wchar_t* tab = wcschr(lines[i], L'\t');
        if (i == 0) FillRect(dc, &r, bar);
        if (i == sel) FillRect(dc, &r, hi);
        RECT t{r.left + 28, r.top, r.right - 28, r.bottom};
        HGDIOBJ oldFont = SelectObject(dc, tab || i == 0 ? font : small);
        SetTextColor(dc, tab || i == 0 ? RGB(255, 255, 255) : RGB(170, 180, 200));
        DrawTextW(dc, lines[i], tab ? int(tab - lines[i]) : -1, &t, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
        if (tab) DrawTextW(dc, tab + 1, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        SelectObject(dc, oldFont);
    }
    GdiFlush();
    memcpy(out, bits, size_t(w) * h * 4);
    SelectObject(dc, oldBm);
    DeleteObject(font); DeleteObject(small); DeleteObject(bg); DeleteObject(hi); DeleteObject(bar); DeleteObject(bm); DeleteDC(dc);
    return true;
}
} // namespace gti::recomp
#else
namespace gti::recomp {
int host_display_hz() { return 0; }
bool host_set_refresh(int) { return false; }
void host_restore_refresh() {}
bool host_wait_refresh() { return false; }
bool host_menu_bitmap(const wchar_t* const*, int, int, int, int, uint32_t*) { return false; }
void host_restart() {}
void host_report(const char*) {}
void host_sleep_us(uint64_t us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
void host_thread_priority_high() {}
void host_thread_core(bool) {}
void host_timer_init() {}
}
#endif
