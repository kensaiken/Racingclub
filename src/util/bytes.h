#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace gti {

inline uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
inline uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
inline float befloat(const uint8_t* p) { uint32_t v = be32(p); float f; std::memcpy(&f, &v, 4); return f; }

inline float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1f, m = h & 0x3ff, f;
    if (e == 0) {
        if (m == 0) f = s << 31;
        else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; --e; } m &= 0x3ff; f = (s << 31) | (e << 23) | (m << 13); }
    } else if (e == 31) f = (s << 31) | 0x7f800000 | (m << 13);
    else f = (s << 31) | ((e - 15 + 127) << 23) | (m << 13);
    float r; std::memcpy(&r, &f, 4); return r;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out);

} // namespace gti
