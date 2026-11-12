#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace gti::edge {

// Decompress a Sony Edge Geometry compressed triangle list (as stored in GTI Club .ep3 segments).
// Reverse engineered format:
//   header: u16 numDeltas, u16 deltaBias, u16 seqFlagBytes, u8 deltaBits, u8 pad   (big-endian)
//   seqFlagBytes : one bit per "needed" index, MSB first (0 = next sequential vertex, 1 = explicit)
//   2 bits per triangle, MSB first: 3 = new triangle (3 needed indexes),
//                                    0/1/2 = shares edge (c,a)/(b,c)/(a,b) of the previous triangle
//   byte align, then numDeltas values of deltaBits: explicit[i] = v - bias + explicit[i-8]
// Returns false on malformed input.
bool decompress_indexes(const uint8_t* data, size_t size, uint32_t numIndexes, std::vector<uint32_t>& out);

} // namespace gti::edge
