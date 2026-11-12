#pragma once
#include <cstdint>
#include <vector>

namespace gti::gcm {

enum class PixelKind { DXT1, DXT3, DXT5, RGBA8, Unsupported };

struct MipLevel {
    uint32_t width, height;
    std::vector<uint8_t> data; // DXT blocks as-is, or RGBA8 (deswizzled) rows top-down
};

struct DecodedTexture {
    PixelKind kind = PixelKind::Unsupported;
    bool cubemap = false;
    // faces[0] for 2D; 6 faces for cubemaps (+X,-X,+Y,-Y,+Z,-Z)
    std::vector<std::vector<MipLevel>> faces;
};

// Decode RSX (CellGcmTexture) texel data. `format` includes LN/UN flags.
bool decode(uint8_t format, uint8_t mipmaps, bool cubemap, uint16_t width, uint16_t height,
            const std::vector<uint8_t>& texels, DecodedTexture& out);

} // namespace gti::gcm
