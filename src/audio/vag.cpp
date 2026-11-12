#include "audio/vag.h"
#include "util/bytes.h"
#include <algorithm>

namespace gti::audio {

bool load_vag(const std::string& path, PcmClip& out) {
    std::vector<uint8_t> d;
    if (!read_file(path, d) || d.size() < 0x40 || std::memcmp(d.data(), "VAGp", 4) != 0) return false;
    const uint32_t dataSize = be32(&d[0x0c]);
    out.sampleRate = be32(&d[0x10]);
    if (out.sampleRate < 4000 || out.sampleRate > 96000) out.sampleRate = 22050;
    static const double coef[5][2] = {{0.0, 0.0}, {60.0 / 64, 0.0}, {115.0 / 64, -52.0 / 64}, {98.0 / 64, -55.0 / 64}, {122.0 / 64, -60.0 / 64}};
    size_t end = std::min<size_t>(d.size(), 0x30 + size_t(dataSize));
    out.samples.clear();
    out.samples.reserve((end - 0x30) / 16 * 28);
    double h1 = 0, h2 = 0;
    for (size_t o = 0x30; o + 16 <= end; o += 16) {
        const int predictor = std::min(d[o] >> 4, 4);
        const int shift = d[o] & 0x0f;
        const uint8_t flags = d[o + 1];
        if (flags == 7) break; // end of stream marker
        for (int i = 0; i < 28; ++i) {
            const uint8_t byte = d[o + 2 + i / 2];
            int nib = (i & 1) ? (byte >> 4) : (byte & 0x0f);
            if (nib & 8) nib -= 16;
            double s = double(nib << 12 >> shift) + h1 * coef[predictor][0] + h2 * coef[predictor][1];
            h2 = h1; h1 = s;
            out.samples.push_back(float(std::max(-32768.0, std::min(32767.0, s)) / 32768.0));
        }
    }
    return !out.samples.empty();
}

} // namespace gti::audio
