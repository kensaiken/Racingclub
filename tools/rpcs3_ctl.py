"""Drive the RPCS3 game window: launch with the keyboard profile, send keys, take screenshots.

Keyboard profile 'auto' (config/input_configs/*/auto.yml):
  stick A/D (steer), P = R2 (throttle), O = L2 (brake), X = cross, C = circle, Return = start,
  arrows = d-pad, Q/E = L1/R1, V = triangle, Z = square.
"""
import ctypes, ctypes.wintypes as wt, subprocess, time, os

RPCS3 = r'C:\Juegos\Emuladores\RPCS3-portable\rpcs3.exe'
EBOOT = r'C:/Juegos/Emuladores/RPCS3-portable/dev_hdd0/game/NPEB00060/USRDIR/EBOOT.BIN'
u32 = ctypes.WinDLL('user32', use_last_error=True)

VK = {'A': 0x41, 'D': 0x44, 'W': 0x57, 'S': 0x53, 'P': 0x50, 'O': 0x4F, 'X': 0x58, 'C': 0x43, 'V': 0x56, 'Z': 0x5A,
      'Q': 0x51, 'E': 0x45, 'Return': 0x0D, 'Backspace': 0x08, 'Left': 0x25, 'Up': 0x26, 'Right': 0x27, 'Down': 0x28}
EXT = {'Left', 'Up', 'Right', 'Down'}


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [('wVk', wt.WORD), ('wScan', wt.WORD), ('dwFlags', wt.DWORD), ('time', wt.DWORD), ('dwExtraInfo', ctypes.c_size_t)]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [('ki', KEYBDINPUT), ('pad', ctypes.c_byte * 32)]
    _anonymous_ = ('u',)
    _fields_ = [('type', wt.DWORD), ('u', _U)]


class NotFocused(Exception):
    pass


_game_hwnd = None


def _send(vk, up, ext):
    # safety: never type into anything but the game window (the user may be using the PC)
    global _game_hwnd
    if _game_hwnd is None:
        _game_hwnd = game_window()[0]
    if not _game_hwnd or u32.GetForegroundWindow() != _game_hwnd:
        if up:  # still allow releasing a key so nothing stays held in the game
            pass
        else:
            raise NotFocused('game window is not in the foreground; stopping input')
    # plain virtual-key events (Qt in RPCS3 ignores scancode-only SendInput events)
    u32.keybd_event(vk, 0, (0x0002 if up else 0) | (0x0001 if ext else 0), 0)


def down(k): _send(VK[k], False, k in EXT)
def up(k): _send(VK[k], True, k in EXT)


def tap(k, hold=0.12, after=0.35):
    down(k); time.sleep(hold); up(k); time.sleep(after)


def launch(input_cfg='auto'):
    global _game_hwnd
    _game_hwnd = None
    subprocess.run(['powershell', '-NoProfile', '-Command',
                    "Get-Process rpcs3 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '*RPCS3-portable*' } | Stop-Process -Force"])
    time.sleep(2)
    subprocess.Popen([RPCS3, '--no-gui', '--config', os.path.join(os.path.dirname(RPCS3), 'config', 'auto_config.yml'), '--input-config', input_cfg, EBOOT], cwd=os.path.dirname(RPCS3))


def game_window():
    """Largest visible top-level window of the RPCS3 process."""
    pid = int(subprocess.run(['powershell', '-NoProfile', '-Command',
                              "(Get-Process rpcs3 | Where-Object { $_.Path -like '*RPCS3-portable*' } | Select-Object -First 1).Id"],
                             capture_output=True, text=True).stdout.strip())
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, _):
        p = wt.DWORD()
        u32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u32.IsWindowVisible(h):
            r = wt.RECT()
            u32.GetWindowRect(h, ctypes.byref(r))
            found.append(((r.right - r.left) * (r.bottom - r.top), h, (r.left, r.top, r.right, r.bottom)))
        return True
    u32.EnumWindows(cb, 0)
    found.sort(reverse=True)
    return found[0][1:] if found else (None, None)


def focus():
    global _game_hwnd
    h, _ = game_window()
    _game_hwnd = h                     # the window changes every time the emulator is relaunched
    if not h:
        return False
    # allow SetForegroundWindow from a background process
    u32.keybd_event(0x12, 0, 0, 0); u32.keybd_event(0x12, 0, 2, 0)
    u32.ShowWindow(h, 9)
    u32.SetForegroundWindow(h)
    time.sleep(0.3)
    return u32.GetForegroundWindow() == h


def shot(path):
    from PIL import ImageGrab
    h, rect = game_window()
    if not h:
        return None
    img = ImageGrab.grab(bbox=rect, all_screens=True)
    img.save(path)
    return path
