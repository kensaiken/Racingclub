#include "recomp/settings.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gti::recomp {
namespace {
const char* const kFile = "gtiport.ini";
struct Field { const char* name; int Settings::* p; int lo, hi; };
const Field kFields[] = {
    {"pantalla_completa", &Settings::fullscreen, 0, 2}, {"resolucion_interna", &Settings::scale, 1, 3}, {"imagenes_por_segundo", &Settings::gameHz, 60, 144},
    {"pantalla_120hz", &Settings::refresh, 0, 1},        {"idioma", &Settings::language, 1, 5},          {"contador_fps", &Settings::showFps, 0, 1},
    {"vsync", &Settings::vsync, 0, 1},
    {"sonido", &Settings::sound, 0, 1},
};
}

Settings& settings() {
    static Settings s;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (FILE* f = std::fopen(kFile, "r")) {
            char line[128];
            while (std::fgets(line, sizeof line, f)) {
                char* eq = std::strchr(line, '=');
                if (!eq) continue;
                *eq = 0;
                for (const Field& fd : kFields)
                    if (!std::strcmp(line, fd.name)) s.*fd.p = std::clamp(std::atoi(eq + 1), fd.lo, fd.hi);
            }
            std::fclose(f);
        }
        if (s.gameHz != 72 && s.gameHz != 120 && s.gameHz != 144) s.gameHz = 60;
        if (const char* e = std::getenv("GAMEHZ")) if (std::atoi(e) > 0) s.gameHz = std::atoi(e);
        if (const char* e = std::getenv("ESCALA")) s.scale = std::clamp(std::atoi(e), 1, 3);
    }
    return s;
}

void settings_save() {
    const Settings& s = settings();
    FILE* f = std::fopen(kFile, "w");
    if (!f) return;
    for (const Field& fd : kFields) std::fprintf(f, "%s=%d\n", fd.name, s.*fd.p);
    std::fclose(f);
}
} // namespace gti::recomp
