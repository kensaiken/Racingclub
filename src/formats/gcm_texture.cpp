#include "formats/gcm_texture.h"
#include <algorithm>

namespace gti::gcm {

namespace {
constexpr uint8_t kLN = 0x20, kUN = 0x40;

// RSX swizzle (Morton order, x bit first, extra bits of the larger dimension appended)
uint32_t swizzle(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t off = 0, bit = 0;
    for (uint32_t i = 0; (1u << i) < w || (1u << i) < h; ++i) {
        if ((1u << i) < w) off |= ((x >> i) & 1u) << bit++;
        if ((1u << i) < h) off |= ((y >> i) & 1u) << bit++;
    }
    return off;
}
} // namespace

bool decode(uint8_t format, uint8_t mipmaps, bool cubemap, uint16_t width, uint16_t height,
            const std::vector<uint8_t>& texels, DecodedTexture& out) {
    const uint8_t base = format & uint8_t(~(kLN | kUN));
    const bool linear = (format & kLN) != 0;
    out = {};
    out.cubemap = cubemap;
    uint32_t bpb = 0; // bytes per 4x4 block for DXT
    switch (base) {
        case 0x86: out.kind = PixelKind::DXT1; bpb = 8; break;
        case 0x87: out.kind = PixelKind::DXT3; bpb = 16; break;
        case 0x88: out.kind = PixelKind::DXT5; bpb = 16; break;
        case 0x85: case 0x81: out.kind = PixelKind::RGBA8; break;
        default: out.kind = PixelKind::Unsupported; return false;
    }
    const int faces = cubemap ? 6 : 1;
    const int mips = std::max<int>(1, mipmaps);
    size_t pos = 0;
    out.faces.resize(faces);
    for (int f = 0; f < faces; ++f) {
        uint32_t w = width, h = height;
        for (int m = 0; m < mips; ++m) {
            MipLevel lv{w, h, {}};
            if (bpb) {
                const size_t sz = size_t(std::max(1u, (w + 3) / 4)) * std::max(1u, (h + 3) / 4) * bpb;
                if (pos + sz > texels.size()) return !out.faces[0].empty();
                lv.data.assign(texels.begin() + pos, texels.begin() + pos + sz);
                pos += sz;
            } else {
                const uint32_t bpp = base == 0x85 ? 4 : 1;
                const size_t sz = size_t(w) * h * bpp;
                if (pos + sz > texels.size()) return !out.faces[0].empty();
                const uint8_t* src = texels.data() + pos;
                lv.data.resize(size_t(w) * h * 4);
                for (uint32_t y = 0; y < h; ++y)
                    for (uint32_t x = 0; x < w; ++x) {
                        const uint32_t i = linear ? (y * w + x) : swizzle(x, y, w, h);
                        uint8_t* d = &lv.data[(size_t(y) * w + x) * 4];
                        if (bpp == 4) { // A8R8G8B8 -> RGBA
                            const uint8_t* s = src + size_t(i) * 4;
                            d[0] = s[1]; d[1] = s[2]; d[2] = s[3]; d[3] = s[0];
                        } else {
                            d[0] = d[1] = d[2] = src[i]; d[3] = 255;
                        }
                    }
                pos += sz;
            }
            out.faces[f].push_back(std::move(lv));
            w = std::max(1u, w / 2); h = std::max(1u, h / 2);
        }
        // cube faces are 128-byte aligned on RSX
        if (cubemap) pos = (pos + 127) & ~size_t(127);
    }
    return true;
}

} // namespace gti::gcm
