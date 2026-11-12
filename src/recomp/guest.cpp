#include "recomp/guest.h"
#include "util/bytes.h"
#include <cstdio>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace gti::recomp {

bool Guest::init(const std::string& path) {
    std::vector<uint8_t> d;
    if (!read_file(path, d) || d.size() < 0x40 || d[0] != 0x7f || d[1] != 'E') return false;
#ifdef _WIN32
    // 4 GB of address space; pages only take RAM when touched
    mem = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x100000000ull, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#endif
    if (!mem) return false;
    auto u16 = [&](size_t o) { return uint16_t(d[o] << 8 | d[o + 1]); };
    auto u32 = [&](size_t o) { return uint32_t(d[o]) << 24 | uint32_t(d[o + 1]) << 16 | uint32_t(d[o + 2]) << 8 | d[o + 3]; };
    auto u64 = [&](size_t o) { return uint64_t(u32(o)) << 32 | u32(o + 4); };
    const uint64_t phoff = u64(0x20);
    const uint16_t phnum = u16(0x38);
    for (int i = 0; i < phnum; ++i) {
        const size_t o = size_t(phoff) + size_t(i) * 0x38;
        if (u32(o) != 1) continue;                       // PT_LOAD
        const uint64_t off = u64(o + 8), va = u64(o + 16), fsz = u64(o + 32);
        if (va + fsz > 0x100000000ull || off + fsz > d.size()) continue;
        std::memcpy(mem + va, d.data() + off, size_t(fsz));
    }
    return true;
}

bool Guest::loadSnapshot(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[4];
    uint32_t n = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "PS3S", 4) != 0 || std::fread(&n, 4, 1, f) != 1) { std::fclose(f); return false; }
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t hdr[2];
        if (std::fread(hdr, 4, 2, f) != 2) break;
        std::fread(mem + hdr[0], 1, hdr[1], f);
    }
    std::fclose(f);
    return true;
}

void Guest::prepare(PPU& c) const {
    c.mem = mem;
    c.r[1] = stackTop - 0x100;
    c.r[2] = toc;
    c.lr = 0;
    c.missing = 0;
    c.unimplAt = 0;
}

} // namespace gti::recomp
