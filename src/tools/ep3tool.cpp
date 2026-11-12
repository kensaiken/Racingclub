// ep3tool: validate / summarize GTI Club .ep3 packages.
//   ep3tool <file.ep3|directory> [-v]
#include "formats/ep3.h"
#include "formats/gcm_texture.h"
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace gti;

static int g_files = 0, g_fail = 0, g_tex = 0, g_texbad = 0, g_ent = 0, g_seg = 0, g_warn = 0;
static size_t g_tris = 0;

static void process(const fs::path& p, bool verbose) {
    ep3::Package pkg;
    std::string err;
    ++g_files;
    if (!pkg.load(p.string(), &err)) { ++g_fail; std::printf("FAIL %s: %s\n", p.string().c_str(), err.c_str()); return; }
    for (auto& [idx, t] : pkg.textures) {
        gcm::DecodedTexture dt;
        ++g_tex;
        if (!gcm::decode(t.format, t.mipmaps, t.cubemap, t.width, t.height, t.texels, dt)) ++g_texbad;
    }
    size_t tris = 0, segs = 0;
    for (auto& e : pkg.entities) { for (auto& m : e.meshes) { tris += m.indices.size() / 3; ++segs; } }
    g_ent += int(pkg.entities.size()); g_seg += int(segs); g_tris += tris; g_warn += int(pkg.warnings.size());
    if (verbose) {
        std::printf("%s: items=%zu tex=%zu mat=%zu ent=%zu seg=%zu tris=%zu warn=%zu\n", p.filename().string().c_str(),
                    pkg.items.size(), pkg.textures.size(), pkg.materials.size(), pkg.entities.size(), segs, tris, pkg.warnings.size());
        for (auto& w : pkg.warnings) std::printf("   warn: %s\n", w.c_str());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: ep3tool <file|dir> [-v]\n"); return 1; }
    bool verbose = argc > 2 && std::string(argv[2]) == "-v";
    fs::path root = fs::u8path(argv[1]);
    if (fs::is_directory(root)) {
        for (auto& de : fs::recursive_directory_iterator(root))
            if (de.is_regular_file() && de.path().extension() == ".ep3") process(de.path(), verbose);
    } else process(root, true);
    std::printf("files=%d failed=%d textures=%d (undecodable %d) entities=%d segments=%d triangles=%zu warnings=%d\n",
                g_files, g_fail, g_tex, g_texbad, g_ent, g_seg, g_tris, g_warn);
    return g_fail ? 2 : 0;
}
