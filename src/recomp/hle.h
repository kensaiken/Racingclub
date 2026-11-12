#pragma once
// Host implementations of the PS3 system library imports used by GTI Club (see docs/fase_recomp_imports.md).
#include "recomp/ppu.h"

namespace gti::recomp {

void hle_install();
void hle_dump_counts();   // HLECOUNT=1: print per-import call counts
// optional extension consulted before the built-in stubs (module, function name); true = handled
using HleExt = bool (*)(PPU&, const char* module, const char* name);
extern HleExt g_hleExt;   // routes calls to import stubs to the host implementations

} // namespace gti::recomp
