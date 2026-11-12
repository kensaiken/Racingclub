#include "formats/edge_indexes.h"
#include "util/bytes.h"

namespace gti::edge {

namespace {
struct BitReader {
    const uint8_t* p; size_t size; size_t bit;
    bool ok = true;
    uint32_t read(unsigned n) {
        uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i) {
            size_t byte = bit >> 3;
            if (byte >= size) { ok = false; return 0; }
            v = (v << 1) | ((p[byte] >> (7 - (bit & 7))) & 1);
            ++bit;
        }
        return v;
    }
    void align() { bit = (bit + 7) & ~size_t(7); }
};
} // namespace

bool decompress_indexes(const uint8_t* data, size_t size, uint32_t numIndexes, std::vector<uint32_t>& out) {
    if (size < 8) return false;
    const uint32_t numDeltas = be16(data + 0);
    const int32_t bias = be16(data + 2);
    const uint32_t flagBytes = be16(data + 4);
    const uint32_t deltaBits = data[6];
    if (8 + flagBytes > size || deltaBits > 24) return false;
    const uint8_t* flags = data + 8;
    const uint32_t numTris = numIndexes / 3;

    BitReader br{data, size, (8 + flagBytes) * 8};
    std::vector<uint8_t> codes(numTris);
    for (uint32_t t = 0; t < numTris; ++t) codes[t] = uint8_t(br.read(2));
    br.align();
    std::vector<int32_t> expl(numDeltas);
    for (uint32_t i = 0; i < numDeltas; ++i)
        expl[i] = int32_t(br.read(deltaBits)) - bias + (i >= 8 ? expl[i - 8] : 0);
    if (!br.ok) return false;

    uint32_t flagIdx = 0, explIdx = 0, next = 0;
    bool bad = false;
    auto get = [&]() -> uint32_t {
        if ((flagIdx >> 3) >= flagBytes) { bad = true; return 0; }
        const uint32_t f = (flags[flagIdx >> 3] >> (7 - (flagIdx & 7))) & 1;
        ++flagIdx;
        if (!f) return next++;
        if (explIdx >= numDeltas) { bad = true; return 0; }
        const int32_t v = expl[explIdx++];
        if (v < 0) { bad = true; return 0; }
        return uint32_t(v);
    };

    out.clear();
    out.reserve(numTris * 3);
    uint32_t a = 0, b = 0, c = 0;
    for (uint32_t t = 0; t < numTris; ++t) {
        const uint8_t code = codes[t];
        uint32_t i0, i1, i2;
        if (code == 3 || t == 0) {
            i0 = get(); i1 = get(); i2 = get();
        } else {
            uint32_t p, q;
            switch (code) {
                case 0: p = c; q = a; break;
                case 1: p = b; q = c; break;
                default: p = a; q = b; break;
            }
            i0 = q; i1 = p; i2 = get();
        }
        if (bad) return false;
        out.push_back(i0); out.push_back(i1); out.push_back(i2);
        a = i0; b = i1; c = i2;
    }
    return true;
}

} // namespace gti::edge
