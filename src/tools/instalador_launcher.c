// "Instalar Racing Club+.exe": opens the graphical installer (instalador.pyw next to this file) with the Python bundled
// in toolchain\python, or with the system's pythonw when that folder is missing. No console window.
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdLine, int show) {
    (void)inst; (void)prev; (void)cmdLine; (void)show;
    wchar_t dir[MAX_PATH], py[MAX_PATH], cmd[3 * MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return 1;
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0;
    swprintf(py, MAX_PATH, L"%ls\\toolchain\\python\\pythonw.exe", dir);
    if (GetFileAttributesW(py) == INVALID_FILE_ATTRIBUTES) wcscpy(py, L"pythonw.exe");
    swprintf(cmd, 3 * MAX_PATH, L"\"%ls\" \"%ls\\instalador.pyw\"", py, dir);
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"No se puede abrir el instalador: falta Python.\n\nInstala Python 3 desde python.org o usa el paquete completo (carpeta toolchain).",
                    L"Racing Club+", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
