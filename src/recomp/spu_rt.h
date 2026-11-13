#pragma once
// Runtime interface of SPU programs statically recompiled to C++ by tools/spu2cpp.py.
// The instruction helpers the generated code uses are in spu_ops.h (needs SSSE3/SSE4.1).
#include "recomp/spu.h"

namespace gti::recomp::spurt {

// A register: four host-order 32-bit words in SPU order (word 0 = preferred slot).
struct Q { uint32_t w[4]; };

// SPUPROF=1: trampoline entries per (program, chunk), printed by the job manager
extern bool g_spuProf;
extern uint64_t g_spuProfCnt[64][256];
// entries into the slow (every instruction is an entry point) variant: indirect branches to unforeseen targets
extern uint64_t g_spuSlowEntries;
void spu_slow_note(uint32_t ea, uint32_t pc);   // SPUPROF=1: where the slow variant is entered ([spuslow] report)
void spu_slow_report();

// A chunk runs from pc until it leaves its 1 KB of code. Result: next pc, | 0x80000000 = the interpreter must
// execute the instruction at that pc, | 0x40000000 (fast chunks only) = pc is not an entry point of the fast chunk.
using FastChunk = uint32_t (*)(Q* __restrict, Spu&, uint32_t pc);
using SlowChunk = uint32_t (*)(Q* __restrict, Spu&, uint32_t pc, bool single);   // single: run one instruction

// one recompiled program: fn runs from s.pc until it needs the interpreter (stop, halt, unknown word, ...)
struct Compiled { uint32_t eaBin, lsBase, size; void (*fn)(Spu&); const SlowChunk* slow; };
const Compiled* find_compiled(uint32_t eaBin);
const Compiled* compiled_at(int index);   // nullptr past the end

} // namespace gti::recomp::spurt
