#include "render/gl_loader.h"
#include <SDL3/SDL.h>

#define GTI_GL_DEFINE(ret, name, ...) PFN_##name name = nullptr;
GTI_GL_FUNCS(GTI_GL_DEFINE)
#undef GTI_GL_DEFINE

namespace gti::gl {
bool load() {
    bool ok = true;
#define GTI_GL_LOAD(ret, name, ...)                                                   \
    name = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress(#name));                \
    if (!name) { SDL_Log("GL: missing %s", #name); ok = false; }
    GTI_GL_FUNCS(GTI_GL_LOAD)
#undef GTI_GL_LOAD
    return ok;
}
} // namespace gti::gl
