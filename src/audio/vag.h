#pragma once
// Sony PS-ADPCM (.vag) decoder.
#include <cstdint>
#include <string>
#include <vector>

namespace gti::audio {

struct PcmClip {
    std::vector<float> samples; // mono, -1..1
    uint32_t sampleRate = 22050;
};

bool load_vag(const std::string& path, PcmClip& out);

} // namespace gti::audio
