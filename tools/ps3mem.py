"""Read-only access to the PS3 address space of a running RPCS3 (Windows).

Only reads memory (ReadProcessMemory); never writes into the emulator.
"""
import ctypes, ctypes.wintypes as wt, struct, subprocess

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
PROCESS_VM_READ, PROCESS_QUERY_INFORMATION = 0x10, 0x400
MEM_COMMIT = 0x1000


class MBI(ctypes.Structure):
    _fields_ = [('BaseAddress', ctypes.c_void_p), ('AllocationBase', ctypes.c_void_p), ('AllocationProtect', wt.DWORD),
                ('PartitionId', wt.WORD), ('RegionSize', ctypes.c_size_t), ('State', wt.DWORD), ('Protect', wt.DWORD), ('Type', wt.DWORD)]


k32.OpenProcess.restype = wt.HANDLE
k32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI), ctypes.c_size_t]
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def rpcs3_pid():
    out = subprocess.run(['powershell', '-NoProfile', '-Command',
                          "(Get-Process rpcs3 | Where-Object { $_.Path -like '*RPCS3-portable*' } | Select-Object -First 1).Id"],
                         capture_output=True, text=True).stdout.strip()
    return int(out)


class Proc:
    def __init__(self, pid=None):
        self.pid = pid or rpcs3_pid()
        self.h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, self.pid)
        if not self.h:
            raise OSError(ctypes.get_last_error())
        self.base = None

    def read(self, addr, n):
        buf = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            return None
        return buf.raw[:got.value]

    def regions(self):
        addr, mbi = 0, MBI()
        while k32.VirtualQueryEx(self.h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
            base = mbi.BaseAddress or 0
            if mbi.State == MEM_COMMIT and not (mbi.Protect & 0x100) and mbi.Protect not in (0, 1):
                yield base, mbi.RegionSize
            addr = base + mbi.RegionSize
            if addr >= 1 << 47:
                break

    # PS3 virtual addresses -----------------------------------------------------------
    def find_base(self, pattern, ps3_addr):
        """Locate RPCS3's vm base: host address where `pattern` (bytes at ps3_addr) sits, 4 GB aligned."""
        for base, size in self.regions():
            if size > 1 << 30:
                continue
            data = self.read(base, size)
            if not data:
                continue
            i = data.find(pattern)
            while i >= 0:
                cand = base + i - ps3_addr
                if cand % (1 << 32) == 0 or cand % (1 << 16) == 0:
                    self.base = cand
                    return cand
                i = data.find(pattern, i + 1)
        return None

    def r(self, ps3, n):
        return self.read(self.base + ps3, n)

    def f32(self, ps3, n=1):
        d = self.r(ps3, 4 * n)
        return struct.unpack('>%df' % n, d) if d else None

    def u32(self, ps3):
        d = self.r(ps3, 4)
        return struct.unpack('>I', d)[0] if d else None
