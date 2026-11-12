#pragma once
// Guest (PS3) address space for recompiled code: 4 GB reservation + loading the decrypted EBOOT's segments.
#include "recomp/ppu.h"
#include <string>

namespace gti::recomp {

struct Guest {
    uint8_t* mem = nullptr;
    uint32_t toc = 0xc6f210;          // r2 for every function of this EBOOT
    uint32_t stackTop = 0xd0100000;   // r1
    bool init(const std::string& ebootElf);   // reserves memory and copies PT_LOAD segments
    void prepare(PPU& c) const;               // registers for a call from the host
    bool loadSnapshot(const std::string& path); // PS3 memory copied from RPCS3 (tools/ps3snap.py)
};

} // namespace gti::recomp
