#pragma once
// RSX (NV47 / "Curie") renderer on OpenGL 4.5 for the fully recompiled game: consumes the NV4097 methods the
// command processor in lv2.cpp decodes, translates the RSX vertex / fragment microcode to GLSL and shows the
// display buffers in an SDL window. Every call comes from the RSX command thread (which owns the GL context).
#include <cstdint>

namespace gti::recomp::rsx {

struct Config {
    uint8_t* mem = nullptr;        // guest address space
    uint32_t localAddr = 0;        // EA of RSX local memory (offset 0)
    uint32_t ioAddr = 0;           // EA of IO offset 0 (main memory mapped for the RSX)
};

bool enabled();                                       // RSX=0 in the environment disables drawing (headless)
void init(const Config& cfg);
void method(uint32_t m, uint32_t arg);                // every NV4097 method (subchannel 0), in stream order
void set_display_buffer(uint32_t id, uint32_t offset, uint32_t pitch, uint32_t w, uint32_t h);
void flip(uint32_t bufferId);                         // shows a display buffer and pumps window events
bool quit_requested();

} // namespace gti::recomp::rsx
