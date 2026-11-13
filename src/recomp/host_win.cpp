// Host display helpers for frame pacing. The game runs at 60 Hz and paces itself on the vblank; if the emulated
// vblank is a free-running timer and the monitor refreshes at something that is not a multiple of 60 (144 Hz),
// every image stays on screen for an irregular number of refreshes and 60 fps look choppy. These helpers let the
// vblank thread follow the real display: host_display_hz() / host_set_refresh() and host_wait_refresh().
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
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
        KmtOpenAdapterFromHdc o{};
        o.hDc = GetDC(nullptr);
        state = o.hDc && D3DKMTOpenAdapterFromHdc(&o) == 0 ? 1 : -1;
        if (o.hDc) ReleaseDC(nullptr, o.hDc);
        w.hAdapter = o.hAdapter; w.vidPnSourceId = o.vidPnSourceId;
    }
    if (state < 0) return false;
    if (D3DKMTWaitForVerticalBlankEvent(&w) != 0) { state = -1; return false; }
    return true;
}
} // namespace gti::recomp
#else
namespace gti::recomp {
int host_display_hz() { return 0; }
bool host_set_refresh(int) { return false; }
void host_restore_refresh() {}
bool host_wait_refresh() { return false; }
}
#endif
