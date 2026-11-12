// GTI Club PC port - milestone 1: native model viewer for the original PS3 game data.
//
// Usage: gtiport.exe [data_dir] [--file some.ep3]
//   data_dir defaults to auto-detection of ...\NPEB00060\USRDIR\data
// Controls:
//   Left/Right   previous/next car          PageUp/PageDown  previous/next .ep3 of the whole game
//   Mouse drag   orbit   Right drag  pan    Wheel  zoom      R  reset camera
//   L  toggle lights   F  wireframe   F11  fullscreen   Esc  quit
//   T  load the Cote d'Azur track   C  free-fly camera (WASD move, Q/E down/up, Shift fast, mouse drag look)
#include "formats/ep3.h"
#include "util/bytes.h"
#include "render/gl_loader.h"
#include "render/renderer.h"
#include "game/collision.h"
#include "game/vehicle.h"
#ifdef GTI_ORIG_PHYSICS
#include "game/orig_physics.h"
#endif
#include "audio/engine_audio.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace gti;

namespace {

bool opt_lights_cli = false;
bool cli_fly = false;
float cli_eye[3]{0, 5, 0}, cli_look[3]{0, 5, 10};

struct Mat4 { float m[16]; };

Mat4 perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 r{};
    float f = 1.0f / std::tan(fovy * 0.5f);
    r.m[0] = f / aspect; r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.0f;
    r.m[14] = 2.0f * zf * zn / (zn - zf);
    return r;
}
Mat4 look_at(const float* e, const float* c, const float* up) {
    float f[3] = {c[0] - e[0], c[1] - e[1], c[2] - e[2]};
    float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    for (float& v : f) v /= fl;
    float s[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
    float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    for (float& v : s) v /= sl;
    float u[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
    Mat4 r{};
    r.m[0] = s[0]; r.m[4] = s[1]; r.m[8] = s[2];
    r.m[1] = u[0]; r.m[5] = u[1]; r.m[9] = u[2];
    r.m[2] = -f[0]; r.m[6] = -f[1]; r.m[10] = -f[2];
    r.m[12] = -(s[0] * e[0] + s[1] * e[1] + s[2] * e[2]);
    r.m[13] = -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]);
    r.m[14] = (f[0] * e[0] + f[1] * e[1] + f[2] * e[2]);
    r.m[15] = 1.0f;
    return r;
}
Mat4 mul(const Mat4& a, const Mat4& b) { // column-major a*b
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}

fs::path find_data_dir(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--file" || a == "--orig") { ++i; continue; }
        if (!a.empty() && a[0] != '-' && fs::is_directory(fs::u8path(a))) return fs::u8path(a);
    }
    std::vector<fs::path> bases;
    if (const char* bp = SDL_GetBasePath()) bases.push_back(fs::u8path(bp));
    bases.push_back(fs::current_path());
    for (auto b : bases) {
        for (int up = 0; up < 5; ++up) {
            for (auto rel : {"data", "NPEB00060/USRDIR/data", "USRDIR/data"}) {
                fs::path p = b / rel;
                if (fs::is_directory(p / "cars")) return p;
            }
            b = b.parent_path();
        }
    }
    return {};
}

// Pull a float list out of the first <tag>...</tag> found after `anchor` in an Emmersion XML file.
std::vector<float> xml_floats(const std::string& xml, const std::string& anchor, const std::string& tag) {
    size_t a = xml.find(anchor);
    if (a == std::string::npos) return {};
    size_t s = xml.find("<" + tag + ">", a);
    if (s == std::string::npos) return {};
    s += tag.size() + 2;
    size_t e = xml.find("</" + tag + ">", s);
    std::vector<float> out;
    const char* c = xml.c_str() + s;
    const char* end = xml.c_str() + e;
    while (c < end) {
        char* nx = nullptr;
        float v = std::strtof(c, &nx);
        if (nx == c) { ++c; continue; }
        out.push_back(v);
        c = nx;
    }
    return out;
}

float cli_ld[3]{-1, -1, -1};
bool cli_nolm = false;
int cli_drive = -1;
bool cli_origSnap = false;
std::string cli_orig;        // --orig <snapshot>: original game physics (recompiled PPU code)
int cli_cam = -1;
bool cli_mute = false;
float cli_sim = 0.f, cli_simSteer = 0.f, cli_simRev = 1e9f;

// Level environment: lights from the level package, fog / sun intensity from the .evp project.
void setup_level(render::Renderer& r, const fs::path& dir) {
    render::Environment& env = r.env;
    ep3::Package lvl;
    if (lvl.load((dir / "cote d'azur.ep3").string())) {
        // geometry here is HUD map quads + the animated train: only the lights are used for now
    }
    {
        const fs::path tso = dir.parent_path().parent_path() / "tracksideobjects";
        ep3::Package sky, sea;
        if (sky.load((tso / "sky" / "sky.ep3").string())) { r.addPackage(sky, 2); r.setGroupSky(2); }
        if (sea.load((tso / "sea" / "sea.ep3").string())) { r.addPackage(sea, 3); r.setGroupReflective(3, 1.0f); }
        const ep3::Light* sun = nullptr;
        std::vector<const ep3::Light*> dirs;
        for (auto& l : lvl.lights) if (l.directional()) dirs.push_back(&l);
        if (!dirs.empty()) sun = dirs[0];
        if (sun) {
            float d[3] = {sun->matrix[8], sun->matrix[9], sun->matrix[10]};
            float n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (n > 0) for (int k = 0; k < 3; ++k) env.sunDir[k] = d[k] / n;
            for (int k = 0; k < 3; ++k) env.sunColour[k] = sun->colour[k];
        }
        if (dirs.size() > 1) for (int k = 0; k < 3; ++k) env.ambient[k] = dirs[1]->colour[k] * 0.6f;
    }
    std::vector<uint8_t> raw;
    if (gti::read_file((dir / "cote d'azur.evp").string(), raw)) {
        std::string xml(raw.begin(), raw.end());
        auto fn = xml_floats(xml, "<EmSgSceneNode>", "FogNear"), ff = xml_floats(xml, "<EmSgSceneNode>", "FogFar"),
             fm = xml_floats(xml, "<EmSgSceneNode>", "FogMax"), fc = xml_floats(xml, "<EmSgSceneNode>", "FogColour"),
             si = xml_floats(xml, "<EmSgSceneNode>", "SunIntensity");
        if (!fn.empty() && !ff.empty()) { env.fogNear = fn[0]; env.fogFar = ff[0]; }
        if (!fm.empty()) env.fogMax = fm[0];
        if (fc.size() >= 3) for (int k = 0; k < 3; ++k) { env.fogColour[k] = fc[k]; env.skyColour[k] = fc[k]; }
        if (!si.empty()) for (int k = 0; k < 3; ++k) env.sunColour[k] *= si[0];
    }
    if (cli_ld[0] >= 0) for (int k = 0; k < 3; ++k) env.lightData[k] = cli_ld[k];
}

std::string pretty(const fs::path& p) {
    std::string s = p.stem().string();
    return s;
}

} // namespace

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }

    fs::path dataDir = find_data_dir(argc, argv);
    std::string explicitFile, shotFile;
    int startCar = 0;
    float shotYaw = 0.6f, shotPitch = 0.25f;
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--file") explicitFile = argv[i + 1];
        else if (a == "--shot") shotFile = argv[i + 1];
        else if (a == "--car") startCar = std::atoi(argv[i + 1]);
        else if (a == "--yaw") shotYaw = float(std::atof(argv[i + 1]));
        else if (a == "--pitch") shotPitch = float(std::atof(argv[i + 1]));
        else if (a == "--lights") opt_lights_cli = std::atoi(argv[i + 1]) != 0;
        else if (a == "--ld" && i + 3 < argc) { for (int k = 0; k < 3; ++k) cli_ld[k] = float(std::atof(argv[i + 1 + k])); }
        else if (a == "--nolm") cli_nolm = true;
        else if (a == "--drive") cli_drive = std::atoi(argv[i + 1]);
        else if (a == "--orig" && i + 1 < argc) cli_orig = argv[i + 1];
        else if (a == "--origsnap") cli_origSnap = true;
        else if (a == "--mute") cli_mute = std::atoi(argv[i + 1]) != 0;
        else if (a == "--cam") cli_cam = std::atoi(argv[i + 1]);
        else if (a == "--sim") cli_sim = float(std::atof(argv[i + 1]));
        else if (a == "--simsteer") cli_simSteer = float(std::atof(argv[i + 1]));
        else if (a == "--simrev") cli_simRev = float(std::atof(argv[i + 1]));
        else if ((a == "--eye" || a == "--look") && i + 3 < argc) {
            float* v = a == "--eye" ? cli_eye : cli_look;
            for (int k = 0; k < 3; ++k) v[k] = float(std::atof(argv[i + 1 + k]));
            cli_fly = true;
        }
    }
    if (dataDir.empty() && explicitFile.empty()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GTI Club PC",
            "No encuentro los datos del juego.\nPasa la ruta de la carpeta 'data' (NPEB00060\\USRDIR\\data) como argumento.", nullptr);
        return 1;
    }

    // collect cars and all packages
    std::vector<fs::path> cars, all;
    if (!dataDir.empty()) {
        for (auto& de : fs::recursive_directory_iterator(dataDir)) {
            if (!de.is_regular_file() || de.path().extension() != ".ep3") continue;
            all.push_back(de.path());
            std::string parent = de.path().parent_path().parent_path().filename().string();
            if (parent.rfind("pack_", 0) == 0 && de.path().stem().string().rfind("extras", 0) != 0) cars.push_back(de.path());
        }
        std::sort(cars.begin(), cars.end());
        std::sort(all.begin(), all.end());
    }
    if (!explicitFile.empty()) { all.insert(all.begin(), fs::u8path(explicitFile)); }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);
    SDL_Window* win = SDL_CreateWindow("GTI Club PC", 1600, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        win = SDL_CreateWindow("GTI Club PC", 1600, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    }
    if (!win) { SDL_Log("window: %s", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_Log("GL context: %s", SDL_GetError()); return 1; }
    SDL_GL_MakeCurrent(win, ctx);
    SDL_GL_SetSwapInterval(1);
    if (!gl::load()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GTI Club PC", "OpenGL 3.3 no disponible", win);
        return 1;
    }
    glEnable(GL_MULTISAMPLE);

    render::Renderer renderer;
    std::string err;
    if (!renderer.init(&err)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GTI Club PC", err.c_str(), win);
        return 1;
    }

    // camera state
    float yaw = 0.6f, pitch = 0.25f, distance = 6.0f, target[3]{0, 0.6f, 0};
    float radius = 2.5f;
    render::DrawOptions opt;
    bool fullscreen = false;
    // free-fly camera (C toggles; automatic for big scenes such as tracks)
    bool fly = cli_fly;
    float flyPos[3]{cli_eye[0], cli_eye[1], cli_eye[2]}, flyYaw = 0.f, flyPitch = 0.f;
    if (cli_fly) {
        float dx = cli_look[0] - cli_eye[0], dy = cli_look[1] - cli_eye[1], dz = cli_look[2] - cli_eye[2];
        flyYaw = std::atan2(dx, -dz);
        flyPitch = std::atan2(dy, std::sqrt(dx * dx + dz * dz));
    }
    uint64_t lastTicks = SDL_GetTicksNS();

    bool useCars = !cars.empty() && explicitFile.empty();
    int index = useCars ? std::clamp(startCar, 0, int(cars.size()) - 1) : 0;
    opt.lightsOn = opt_lights_cli;
    opt.lightmaps = !cli_nolm;
    if (!shotFile.empty()) { yaw = shotYaw; pitch = shotPitch; }
    std::string title;
    auto load = [&](const fs::path& p, const std::string& listInfo) {
        ep3::Package pkg;
        std::string e;
        if (!pkg.load(p.string(), &e)) {
            SDL_Log("load %s: %s", p.string().c_str(), e.c_str());
            renderer.clearScene();
            title = "GTI Club PC | ERROR " + p.filename().string() + ": " + e;
        } else {
            renderer.setPackage(pkg);
            renderer.env = render::Environment{};
            if (p.filename() == "afterburn.ep3") setup_level(renderer, p.parent_path());
            for (int k = 0; k < 3; ++k) target[k] = 0.5f * (renderer.boundsMin[k] + renderer.boundsMax[k]);
            float dx = renderer.boundsMax[0] - renderer.boundsMin[0], dy = renderer.boundsMax[1] - renderer.boundsMin[1],
                  dz = renderer.boundsMax[2] - renderer.boundsMin[2];
            radius = std::max(0.25f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
            distance = radius * 2.4f;
            if (radius > 100.f && !cli_fly) { // a level: start flying above the centre
                fly = true;
                flyPos[0] = target[0]; flyPos[1] = renderer.boundsMax[1] + 20.f; flyPos[2] = target[2] + radius * 0.5f;
                flyYaw = 0.f; flyPitch = -0.5f;
            }
            char buf[512];
            std::snprintf(buf, sizeof buf, "GTI Club PC  |  %s  %s  |  %zu triangulos  |  Luces: %s  |  <-/-> coche  RePag/AvPag todos  L luces  F alambre  T circuito  C camara libre  K lightmaps",
                          pretty(p).c_str(), listInfo.c_str(), renderer.triangleCount, opt.lightsOn ? "ON" : "OFF");
            title = buf;
        }
        SDL_SetWindowTitle(win, title.c_str());
    };
    auto reload = [&]() {
        if (useCars) load(cars[index], "(" + std::to_string(index + 1) + "/" + std::to_string(cars.size()) + ")");
        else if (!all.empty()) load(all[index], "(" + std::to_string(index + 1) + "/" + std::to_string(all.size()) + ")");
    };
    reload();
    if (!shotFile.empty()) { yaw = shotYaw; pitch = shotPitch; }

    // ------------------------------------------------------------------ driving
    bool driving = false;
    int driveIdx = 0;
    std::unique_ptr<ep3::Package> trackPkg, levelPkg;
    CollisionWorld world;
    Vehicle car;
#ifdef GTI_ORIG_PHYSICS
    OrigPhysics orig;
    bool useOrig = false;
    std::string origMsg;
    std::vector<std::string> origSnaps;   // one snapshot of the original game per car (N/B switch between them)
    int origIdx = 0;
    std::string origLoaded;
    std::vector<std::array<float, 9>> carGeo;   // per car model: hardpoint x,z of the 4 wheels + radius
#endif
    float camEye[3]{0, 0, 0};
    bool camInit = false;
    SDL_Gamepad* pad = nullptr;
    std::string driveName;
    std::vector<ep3::Camera> replayCams;
    int replayCam = -1;
    float orbitYaw = 0, orbitPitch = 0, fovY = 0.9f;
    int camMode = 1;            // 0 cockpit, 1 chase near, 2 chase far
    float carTop = 1.3f, impactCooldown = 0;
    bool sfxOk = false;
    audio::EngineAudio engineAudio;
    bool audioOk = false;
    auto startDrive = [&](int carIdx) {
        if (cars.empty() || dataDir.empty()) return;
        const int carDelta = carIdx - driveIdx;   // N / B / Backspace
        (void)carDelta;
        driveIdx = (carIdx % int(cars.size()) + int(cars.size())) % int(cars.size());
#ifdef GTI_ORIG_PHYSICS
        if (!cli_orig.empty()) {
            if (origSnaps.empty()) {
                std::error_code ec;
                if (fs::is_directory(cli_orig, ec)) {
                    for (auto& de : fs::directory_iterator(cli_orig, ec))
                        if (de.path().extension() == ".snap") origSnaps.push_back(de.path().string());
                    std::sort(origSnaps.begin(), origSnaps.end());
                } else origSnaps.push_back(cli_orig);
            }
            if (!origSnaps.empty()) {
                origIdx += carDelta;   // N/B move through the snapshots
                origIdx = (origIdx % int(origSnaps.size()) + int(origSnaps.size())) % int(origSnaps.size());
                const std::string& sp = origSnaps[origIdx];
                if (origLoaded != sp) {
                    std::string eb;
                    for (const char* cand : {"re/EBOOT.elf", "../re/EBOOT.elf", "GTiPort/re/EBOOT.elf"})
                        if (fs::exists(cand)) { eb = cand; break; }
                    if (orig.boot(eb.empty() ? "re/EBOOT.elf" : eb, sp)) { useOrig = true; origMsg = "FISICA ORIGINAL " + fs::path(sp).stem().string(); }
                    else { useOrig = false; origMsg = "fisica original no disponible: " + orig.error; SDL_Log("%s", origMsg.c_str()); }
                    origLoaded = sp;
                } else if (orig.ok()) { orig.reset(); useOrig = true; }
                if (orig.ok()) {
                    // the car model whose wheel hardpoints match the original's simulated wheels
                    if (carGeo.empty())
                        for (auto& cp : cars) {
                            std::array<float, 9> gg{};
                            ep3::Package pk;
                            if (pk.load(cp.string())) {
                                static const char* hpn[4] = {"hardPointFL", "hardPointFR", "hardPointRL", "hardPointRR"};
                                for (int wi = 0; wi < 4; ++wi)
                                    if (auto* l = pk.findLocator(hpn[wi])) { gg[2 * wi] = l->matrix[12]; gg[2 * wi + 1] = l->matrix[14]; }
                            }
                            carGeo.push_back(gg);
                        }
                    float best = 1e9f;
                    for (size_t ci = 0; ci < cars.size() && ci < carGeo.size(); ++ci) {
                        float e = 0;
                        for (int wi = 0; wi < 4; ++wi)
                            e += std::fabs(carGeo[ci][2 * wi] - orig.wheels[wi].hubLocal.x) + std::fabs(carGeo[ci][2 * wi + 1] - orig.wheels[wi].hubLocal.z);
                        if (e < best) { best = e; driveIdx = int(ci); }
                    }
                }
            }
        }
#endif
        const fs::path lvlDir = dataDir / "levels" / "cote d'azur";
        if (!trackPkg) {
            trackPkg = std::make_unique<ep3::Package>();
            if (!trackPkg->load((lvlDir / "afterburn.ep3").string())) { trackPkg.reset(); return; }
            levelPkg = std::make_unique<ep3::Package>();
            levelPkg->load((lvlDir / "cote d'azur.ep3").string());
            {
                ep3::Package rc;
                if (rc.load((lvlDir / "replaycameras.ep3").string())) replayCams = rc.cameras;
            }
            world = CollisionWorld{};
            world.addPackage(*trackPkg);
            world.build(4.0f);
        }
        if (!driving) {
            renderer.setPackage(*trackPkg);
            renderer.env = render::Environment{};
            setup_level(renderer, lvlDir);
        }
        renderer.removeGroup(1);
        ep3::Package carPkg;
        if (!carPkg.load(cars[driveIdx].string())) return;
        renderer.addPackage(carPkg, 1);
        {
            // roof height above the cabin centre (ignores antennas / light bars / spoilers at the ends)
            float top = 0;
            for (auto& e : carPkg.entities)
                for (auto& m : e.meshes)
                    for (size_t v = 0; v + 2 < m.positions.size(); v += 3) {
                        const float x = m.positions[v] + e.world[12], y = m.positions[v + 1] * e.world[5] + e.world[13], z = m.positions[v + 2] + e.world[14];
                        if (std::fabs(x) < 0.3f && z > -0.5f && z < 0.3f && y < 1.8f) top = std::max(top, y);
                    }
            carTop = top > 0.8f ? top : 1.3f;
        }
        CarParams cp;
        fs::path phys = cars[driveIdx].parent_path() / (cars[driveIdx].stem().string() + "_phys.xml");
        if (!load_car_params(phys.string(), cp)) SDL_Log("phys xml missing: %s", phys.string().c_str());
        car.setup(cp, carPkg);
        V3 spawn{25.5f, 2.7f, 77.1f};
        float heading = 1.5707963f;
        if (levelPkg) if (auto* l = levelPkg->findLocator("Start_Lower_0")) {
            spawn = {l->matrix[12], l->matrix[13], l->matrix[14]};
            heading = std::atan2(l->matrix[8], l->matrix[10]);
        }
        car.reset(spawn, heading);
#ifdef GTI_ORIG_PHYSICS
        if (!cli_orig.empty()) {
            // a snapshot taken on the start grid is the original start itself; any other moment: put the car on the grid
            if (useOrig && orig.ok() && orig.kmh > 2.0f && !cli_origSnap) orig.place(spawn + V3{0, 0.05f, 0}, heading);
            if (useOrig) {
                car.setPose(orig.origin, orig.axisX, orig.axisY, orig.axisZ);
                for (int wi = 0; wi < 4; ++wi)
                    SDL_Log("%s wheel %d hardpoint (%.3f %.3f %.3f) model hub (%.3f %.3f %.3f) r %.3f | original hub (%.3f %.3f %.3f) r %.3f", cars[driveIdx].stem().string().c_str(), wi,
                            car.wheels[wi].hardpoint.x, car.wheels[wi].hardpoint.y, car.wheels[wi].hardpoint.z, car.wheels[wi].hubLocal.x, car.wheels[wi].hubLocal.y,
                            car.wheels[wi].hubLocal.z, car.wheels[wi].radius, orig.wheels[wi].hubLocal.x, orig.wheels[wi].hubLocal.y, orig.wheels[wi].hubLocal.z, orig.wheels[wi].radius);
            }
        }
#endif
        if (!cli_mute) {
            if (!audioOk) audioOk = engineAudio.open();
            fs::path audioXml = cars[driveIdx].parent_path() / (cars[driveIdx].stem().string() + "_full_audio.xml");
            if (audioOk && !engineAudio.loadCar(audioXml.string(), dataDir.string())) SDL_Log("no engine audio for %s", audioXml.string().c_str());
            if (audioOk && !sfxOk) sfxOk = engineAudio.loadEffects(dataDir.string());
        }
        driving = true; fly = false; camInit = false;
        driveName = cars[driveIdx].stem().string();
    };
#ifdef GTI_ORIG_PHYSICS
    if (!cli_orig.empty() && cli_drive < 0) cli_drive = 0;   // the car comes from the snapshot
#endif
    if (cli_drive >= 0) startDrive(cli_drive);

    bool running = true;
    int frame = 0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keycode k = ev.key.key;
                    if (k == SDLK_ESCAPE) { if (driving) { driving = false; renderer.hiddenGroup = -1; engineAudio.setState(900, 0, 0); renderer.removeGroup(1); reload(); } else running = false; }
                    else if (k == SDLK_G) startDrive(useCars ? index : driveIdx);
                    else if (driving && k == SDLK_N) startDrive(driveIdx + 1);
                    else if (driving && k == SDLK_B) startDrive(driveIdx - 1);
                    else if (driving && k == SDLK_BACKSPACE) startDrive(driveIdx);
                    else if (driving && k == SDLK_V) { camMode = (camMode + 1) % 4; camInit = false; }
#ifdef GTI_ORIG_PHYSICS
                    else if (driving && k == SDLK_O && orig.ok()) { useOrig = !useOrig; origMsg = useOrig ? "FISICA ORIGINAL" : "fisica del port"; if (useOrig) { const V3 f = car.rot.rotate({0, 0, 1}); const V3 o = car.originWorld(); orig.reset(); orig.place(o + V3{0, 0.05f, 0}, std::atan2(f.x, f.z)); car.setPose(orig.origin, orig.axisX, orig.axisY, orig.axisZ); } }
#endif
                    else if (driving && (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN || k == SDLK_L || k == SDLK_C)) {}
                    else if (k == SDLK_RIGHT && !cars.empty()) { if (!useCars) { useCars = true; index = -1; } index = (index + 1) % int(cars.size()); reload(); }
                    else if (k == SDLK_LEFT && !cars.empty()) { if (!useCars) { useCars = true; index = 0; } index = (index + int(cars.size()) - 1) % int(cars.size()); reload(); }
                    else if (k == SDLK_PAGEDOWN && !all.empty()) { if (useCars) { useCars = false; index = -1; } index = (index + 1) % int(all.size()); reload(); }
                    else if (k == SDLK_PAGEUP && !all.empty()) { if (useCars) { useCars = false; index = 0; } index = (index + int(all.size()) - 1) % int(all.size()); reload(); }
                    else if (k == SDLK_L) { opt.lightsOn = !opt.lightsOn; reload(); }
                    else if (k == SDLK_F) opt.wireframe = !opt.wireframe;
                    else if (k == SDLK_K) opt.lightmaps = !opt.lightmaps;
                    else if (k == SDLK_R) { yaw = 0.6f; pitch = 0.25f; distance = radius * 2.4f; }
                    else if (k == SDLK_F11) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); }
                    else if (k == SDLK_T && !dataDir.empty()) {
                        fs::path trk = dataDir / "levels" / "cote d'azur" / "afterburn.ep3";
                        auto it = std::find(all.begin(), all.end(), trk);
                        if (it != all.end()) { useCars = false; index = int(it - all.begin()); reload(); }
                    }
                    else if (k == SDLK_C) {
                        fly = !fly;
                        if (fly) { // start from the current orbit eye
                            flyPos[0] = target[0] + distance * std::cos(pitch) * std::sin(yaw);
                            flyPos[1] = target[1] + distance * std::sin(pitch);
                            flyPos[2] = target[2] + distance * std::cos(pitch) * std::cos(yaw);
                            flyYaw = -yaw; flyPitch = -pitch;
                        }
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                    if (fly) {
                        if (ev.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) {
                            flyYaw += ev.motion.xrel * 0.004f;
                            flyPitch = std::clamp(flyPitch - ev.motion.yrel * 0.004f, -1.55f, 1.55f);
                        }
                    } else if (ev.motion.state & SDL_BUTTON_LMASK) {
                        yaw -= ev.motion.xrel * 0.008f;
                        pitch = std::clamp(pitch + ev.motion.yrel * 0.008f, -1.5f, 1.5f);
                    } else if (ev.motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK)) {
                        float s = distance * 0.0015f;
                        float rx = std::cos(yaw), rz = -std::sin(yaw);
                        target[0] -= rx * ev.motion.xrel * s;
                        target[2] -= rz * ev.motion.xrel * s;
                        target[1] += ev.motion.yrel * s;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    distance = std::clamp(distance * std::pow(0.88f, ev.wheel.y), radius * 0.2f, radius * 50.0f);
                    break;
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    if (driving && (ev.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH || ev.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) { camMode = (camMode + 1) % 4; camInit = false; }
                    else if (driving && ev.gbutton.button == SDL_GAMEPAD_BUTTON_START) startDrive(driveIdx);
                    break;
                case SDL_EVENT_GAMEPAD_ADDED:
                    if (!pad) pad = SDL_OpenGamepad(ev.gdevice.which);
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    if (pad && SDL_GetGamepadID(pad) == ev.gdevice.which) { SDL_CloseGamepad(pad); pad = nullptr; }
                    break;
                default: break;
            }
        }
        int w = 1, h = 1;
        SDL_GetWindowSizeInPixels(win, &w, &h);
        const uint64_t now = SDL_GetTicksNS();
        const float dt = std::min(0.1f, float(now - lastTicks) * 1e-9f);
        lastTicks = now;
        float eye[3], look[3];
        float up[3] = {0, 1, 0};
        float znear, zfar;
        if (driving) {
            CarInput in;
            const bool* ks = SDL_GetKeyboardState(nullptr);
            in.throttle = (ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W]) ? 1.f : 0.f;
            in.brake = (ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S]) ? 1.f : 0.f;
            in.steer = float(int(ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A]) - int(ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D]));
            in.handbrake = ks[SDL_SCANCODE_SPACE];
            if (pad) {
                float rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.f, lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.f;
                {
                    float rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f, ry = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
                    if (std::fabs(rx) < 0.15f) rx = 0;
                    if (std::fabs(ry) < 0.15f) ry = 0;
                    const float a = 1.0f - std::exp(-10.0f * dt);
                    orbitYaw += (rx * 3.14159f - orbitYaw) * a;     // full stick = look behind
                    orbitPitch += (ry * 0.45f - orbitPitch) * a;
                }
                float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
                if (std::fabs(sx) < 0.12f) sx = 0;
                in.throttle = std::max(in.throttle, rt);
                in.brake = std::max(in.brake, lt);
                if (sx != 0) in.steer = -sx;
                in.handbrake = in.handbrake || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
            }
            float stepDt = dt;
            if (cli_sim > 0) {
                static float simClock = 0; simClock += 1.f / 60.f;
                in.throttle = 1.f; in.steer = cli_simSteer;
                if (simClock > cli_simRev && simClock < cli_simRev + 2.5f) { in.throttle = 0; in.brake = 1; in.steer = -cli_simSteer; }
                else if (simClock >= cli_simRev + 2.5f) in.steer = 0;
                stepDt = 1.f / 60.f; cli_sim -= stepDt; if (cli_sim <= 0) cli_sim = -1;
            }
#ifdef GTI_ORIG_PHYSICS
            if (useOrig && orig.ok()) {
                orig.step(stepDt, in);
                if (!orig.ok()) { useOrig = false; origMsg = orig.error; SDL_Log("%s", origMsg.c_str()); }
                car.setPose(orig.origin, orig.axisX, orig.axisY, orig.axisZ);
                car.vel = orig.vel;
                car.angVel = orig.angVel;
                car.rpm = orig.rpm;
                car.speedKmh = orig.kmh;
                car.gear = orig.gear;
                for (int wi = 0; wi < 4; ++wi) {   // wheels exactly as the original simulates them
                    auto& wh = car.wheels[wi];
                    wh.steer = orig.wheels[wi].steer;
                    wh.spin = orig.wheels[wi].spin;
                    wh.contact = true;
                }
            } else
#endif
            car.update(stepDt, in, world);
            if (audioOk) {
                engineAudio.setState(car.rpm, in.throttle, 0.7f);
                impactCooldown -= stepDt;
                if (car.impact > 0 && impactCooldown <= 0) { engineAudio.playImpact(car.impact); impactCooldown = 0.18f; }
                float skid = 0;
                if (car.speedKmh > 12.0f)
                    for (auto& wh : car.wheels)
                        if (wh.contact) skid = std::max({skid, std::clamp((std::fabs(wh.dbgSA) - 7.0f) / 10.0f, 0.0f, 1.0f),
                                                         std::clamp((std::fabs(wh.dbgSR) - 20.0f) / 40.0f, 0.0f, 1.0f)});
                engineAudio.setEffects(car.scrape, skid);
            }
            car.impact = 0;
            if (cli_sim != 0) {
                static FILE* simlog = std::fopen("sim_log.txt", "w");
                static float simT = 0; simT += stepDt;
                static int simN = 0;
                if (simlog && (++simN % 15) == 0)
                    std::fprintf(simlog, "t=%.2f speed=%.1f km/h gear=%d rpm=%.0f pos=(%.1f %.1f %.1f) contacts=%d%d%d%d comp=%.3f\n", simT, car.speedKmh, car.gear, car.rpm,
                                 car.pos.x, car.pos.y, car.pos.z, car.wheels[0].contact, car.wheels[1].contact, car.wheels[2].contact, car.wheels[3].contact, car.wheels[0].compression);
                if (simlog && (simN % 15) == 0) std::fprintf(simlog, "   probes:%s  wheels comp %.3f %.3f %.3f %.3f\n", car.dbgProbes.c_str(), car.wheels[0].compression, car.wheels[1].compression, car.wheels[2].compression, car.wheels[3].compression);
                if (simlog) std::fflush(simlog);
            }
            float carM[16];
            const V3 org = car.originWorld();
            to_matrix(car.rot, org, carM);
            renderer.setGroupTransform(1, carM);
            static const char* wn[4] = {"WheelFL", "WheelFR", "WheelRL", "WheelRR"};
            for (int i = 0; i < 4; ++i) {
                const auto& wh = car.wheels[i];
                const float off = wh.contact ? (wh.compression - car.params.springLength * 0.65f) : -car.params.springLength * 0.35f;
                float Tm[16], Rs[16], Rw[16], Tb[16], tmp[16], tmp2[16], loc[16];
                V3 hubPos = wh.hubLocal + V3{0, off, 0};
#ifdef GTI_ORIG_PHYSICS
                if (useOrig && orig.ok()) hubPos = {wh.hubLocal.x, orig.wheels[i].hubLocal.y, orig.wheels[i].hubLocal.z};   // simulated suspension travel
#endif
                to_matrix(Quat{}, hubPos, Tm);
                to_matrix(Quat::axisAngle({0, 1, 0}, wh.steer), {0, 0, 0}, Rs);
                to_matrix(Quat::axisAngle({1, 0, 0}, wh.spin), {0, 0, 0}, Rw);
                to_matrix(Quat{}, -wh.hubLocal, Tb);
                mat_mul(Tm, Rs, tmp); mat_mul(tmp, Rw, tmp2); mat_mul(tmp2, Tb, loc);
                float full[16]; mat_mul(carM, loc, full);
                renderer.setEntityTransform(1, wn[i], full);
            }
            const V3 cfwd = car.rot.rotate({0, 0, 1});
            V3 flat = normalize(V3{cfwd.x, 0, cfwd.z});
            if (cli_cam >= 0) { camMode = cli_cam % 4; cli_cam = -1; }
            if (camMode == 0) {
                // cockpit: rigid with the car, driver's seat (left-hand drive: car +X is left)
                const V3 e3 = org + car.rot.rotate(V3{0.32f, carTop - 0.30f, -0.05f});
                const V3 l3 = e3 + cfwd * 10.0f;
                const V3 u3 = car.rot.rotate(V3{0, 1, 0});
                eye[0] = e3.x; eye[1] = e3.y; eye[2] = e3.z;
                look[0] = l3.x; look[1] = l3.y; look[2] = l3.z;
                up[0] = u3.x; up[1] = u3.y; up[2] = u3.z;
                znear = 0.05f;
                renderer.hiddenGroup = 1;      // no car shell around the eye: a clean driver's-eye view
            } else if (camMode == 3 && !replayCams.empty()) {
                // cinematic: the closest trackside replay camera films the car (switches with hysteresis)
                renderer.hiddenGroup = -1;
                auto dist2 = [&](int i) { const auto& c = replayCams[i]; float dx = c.pos[0] - org.x, dy = c.pos[1] - org.y, dz = c.pos[2] - org.z; return dx * dx + dy * dy + dz * dz; };
                int best = 0;
                for (int i = 1; i < int(replayCams.size()); ++i) if (dist2(i) < dist2(best)) best = i;
                if (replayCam < 0 || dist2(best) < dist2(replayCam) * 0.6f) replayCam = best;
                const auto& rc = replayCams[replayCam];
                for (int k = 0; k < 3; ++k) eye[k] = rc.pos[k];
                look[0] = org.x; look[1] = org.y + 0.8f; look[2] = org.z;
                // zoom so the car keeps roughly the same size on screen (TV-style), within the camera's lens range
                const float d = std::sqrt(dist2(replayCam));
                fovY = std::clamp(2.0f * std::atan(5.0f / std::max(d, 1.0f)), 0.12f, rc.fovDeg * 3.14159f / 180.0f * 1.6f);
                znear = 0.3f;
            } else {
                renderer.hiddenGroup = -1;
                // the original's chase cameras (data/cars/effects/cameras.ep3), in the car frame (origin at road level):
                //   Camera_Chase (near) at (0, 1.288, -7.062), Camera_ChaseHigh (far) at (0, 2.1015, -8.2353),
                //   both aimed at the ChaseCam locator (0, 2.16, 0); vertical field of view 55.79 degrees
                const bool farCam = camMode == 2;
                const V3 camOff = farCam ? V3{0.0f, 2.1015f, -8.2353f} : V3{0.0f, 1.2880f, -7.0619f};
                const float aimY = 2.16f;
                // right stick orbits the camera around the car
                const float cy = std::cos(orbitYaw), syw = std::sin(orbitYaw);
                const V3 dir{flat.x * cy - flat.z * syw, 0, flat.x * syw + flat.z * cy};
                const V3 upW{0, 1, 0};
                V3 want = org + dir * camOff.z + upW * (camOff.y - orbitPitch * camOff.z);
                const V3 target3 = org + upW * aimY;
                if (!camInit) { camEye[0] = want.x; camEye[1] = want.y; camEye[2] = want.z; camInit = true; }
                const float a = 1.0f - std::exp(-25.0f * stepDt);   // trailing distance measured on PS3 footage: ~0.6-1.5 m at 100-120 km/h
                camEye[0] += (want.x - camEye[0]) * a; camEye[1] += (want.y - camEye[1]) * a; camEye[2] += (want.z - camEye[2]) * a;
                for (int k = 0; k < 3; ++k) eye[k] = camEye[k];
                look[0] = target3.x; look[1] = target3.y; look[2] = target3.z;
                znear = 0.1f;
                fovY = 55.7883f * 3.14159265f / 180.0f;
            }
            zfar = 5000.f;
            static int titleTick = 0;
            if (++titleTick % 10 == 0) {
                char buf[256];
                std::string g = car.gear == 0 ? std::string("R") : std::to_string(car.gear);
                std::string extra;
#ifdef GTI_ORIG_PHYSICS
                if (!origMsg.empty()) {
                    char ob[160];
                    std::snprintf(ob, sizeof ob, "  |  %s%s", origMsg.c_str(), useOrig ? "" : "");
                    extra = ob;
                    if (useOrig) { std::snprintf(ob, sizeof ob, " (%.2f ms)  O cambiar", orig.stepMs); extra += ob; }
                }
#endif
                std::snprintf(buf, sizeof buf, "GTI Club PC  |  %s  |  %3.0f km/h  |  marcha %s  |  %4.0f rpm%s  |  V camara  N/B coche  Retroceso reiniciar  Esc salir",
                              driveName.c_str(), car.speedKmh, g.c_str(), car.rpm, extra.c_str());
                SDL_SetWindowTitle(win, buf);
            }
        } else if (fly) {
            const float fwd[3] = {std::cos(flyPitch) * std::sin(flyYaw), std::sin(flyPitch), -std::cos(flyPitch) * std::cos(flyYaw)};
            const float right[3] = {std::cos(flyYaw), 0.f, std::sin(flyYaw)};
            const bool* ks = SDL_GetKeyboardState(nullptr);
            float speed = (ks[SDL_SCANCODE_LSHIFT] ? 120.f : 25.f) * dt;
            if (ks[SDL_SCANCODE_LCTRL]) speed *= 0.15f;
            float mf = float(ks[SDL_SCANCODE_W] - ks[SDL_SCANCODE_S]), mr = float(ks[SDL_SCANCODE_D] - ks[SDL_SCANCODE_A]),
                  mu = float(ks[SDL_SCANCODE_E] - ks[SDL_SCANCODE_Q]);
            for (int k = 0; k < 3; ++k) flyPos[k] += (fwd[k] * mf + right[k] * mr) * speed;
            flyPos[1] += mu * speed;
            for (int k = 0; k < 3; ++k) { eye[k] = flyPos[k]; look[k] = flyPos[k] + fwd[k]; }
            znear = 0.1f; zfar = 5000.f;
        } else {
            eye[0] = target[0] + distance * std::cos(pitch) * std::sin(yaw);
            eye[1] = target[1] + distance * std::sin(pitch);
            eye[2] = target[2] + distance * std::cos(pitch) * std::cos(yaw);
            for (int k = 0; k < 3; ++k) look[k] = target[k];
            znear = std::max(0.01f, distance * 0.01f); zfar = distance * 100.0f + radius * 10.0f;
        }
        if (!driving || camMode == 0) fovY = 0.9f;
        Mat4 proj = perspective(fovY, float(w) / float(std::max(h, 1)), znear, zfar);
        Mat4 view = look_at(eye, look, up);
        renderer.camRight[0] = view.m[0]; renderer.camRight[1] = view.m[4]; renderer.camRight[2] = view.m[8];
        renderer.camUp[0] = view.m[1]; renderer.camUp[1] = view.m[5]; renderer.camUp[2] = view.m[9];
        Mat4 vp = mul(proj, view);
        renderer.draw(vp.m, eye, w, h, opt);
        if (!shotFile.empty() && cli_sim <= 0 && ++frame == 3) {
            glFinish();
            std::vector<uint8_t> px(size_t(w) * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ABGR8888);
            for (int y = 0; y < h; ++y)
                std::memcpy((uint8_t*)s->pixels + size_t(y) * s->pitch, &px[size_t(h - 1 - y) * w * 4], size_t(w) * 4);
            SDL_SaveBMP(s, shotFile.c_str());
            SDL_DestroySurface(s);
            running = false;
        }
        SDL_GL_SwapWindow(win);
    }
    engineAudio.close();
    if (pad) SDL_CloseGamepad(pad);
    renderer.shutdown();
    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
