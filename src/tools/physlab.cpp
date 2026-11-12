// Headless physics bench: flat ground + walls, scripted inputs, prints the car state.
// usage: physlab <car.ep3> <scenario: steer|wall|glance|slalom>
#include "formats/ep3.h"
#include "game/collision.h"
#include "game/vehicle.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <string>

using namespace gti;

// Replay windows extracted from recordings of the original game (tools/replay_extract.py):
//   W <frames> <vfwd> <vlat> <yawrate> <gear> <steerN> <thr> <brk>
//   <thr> <brk> <hb> <steer>          (one line per 1/60 s frame, raw pad inputs)
// Output: per window "W", then per frame "<vfwd> <vlat> <yawrate>" in the car frame.
static int replay(const char* carPath, const char* winPath, const char* outPath, const char* trackPath) {
    ep3::Package pkg;
    if (!pkg.load(carPath)) return 1;
    std::string cp = carPath;
    CarParams params;
    if (!load_car_params(cp.substr(0, cp.size() - 4) + "_phys.xml", params)) return 1;
    CollisionWorld w;
    ep3::Package track;
    if (trackPath && track.load(trackPath)) {
        w.addPackage(track);
        w.build(4.0f);
    } else {
        trackPath = nullptr;
        const float S = 2000;
        w.addTriangle({-S, 0, -S}, {S, 0, -S}, {S, 0, S});
        w.addTriangle({-S, 0, -S}, {S, 0, S}, {-S, 0, S});
        w.build(16.0f);
    }
    FILE* in = std::fopen(winPath, "r");
    FILE* out = std::fopen(outPath, "w");
    if (!in || !out) return 1;
    char tag[4];
    int n, gear;
    float vf, vl, yr, sn, th, br;
    float pose[13];
    int winIdx = -1;
    while (std::fscanf(in, "%3s %d %f %f %f %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", tag, &n, &vf, &vl, &yr, &gear, &sn, &th, &br,
                       &pose[0], &pose[1], &pose[2], &pose[3], &pose[4], &pose[5], &pose[6], &pose[7], &pose[8], &pose[9], &pose[10], &pose[11], &pose[12]) == 22) {
        ++winIdx;
        Vehicle car;
        car.setup(params, pkg);
        if (trackPath) {
            car.reset({pose[0], pose[1], pose[2]}, 0);
            car.setPose({pose[0], pose[1], pose[2]}, {pose[3], pose[4], pose[5]}, {pose[6], pose[7], pose[8]}, {pose[9], pose[10], pose[11]});
        } else {
            car.reset({0, 0.1f, 0}, 0);
            CarInput rest;
            for (int i = 0; i < 90; ++i) car.update(1.0f / 60.0f, rest, w);   // settle on the springs
        }
        car.setDrivingState(vf, vl, yr, gear, sn, th, br);
        if (trackPath) car.vel.y += pose[12];   // world vertical speed of the original
        std::fprintf(out, "W\n");
        for (int i = 0; i < n; ++i) {
            CarInput ci;
            int hb = 0;
            if (std::fscanf(in, "%f %f %d %f", &ci.throttle, &ci.brake, &hb, &ci.steer) != 4) break;
            ci.handbrake = hb != 0;
            car.update(1.0f / 60.0f, ci, w);
            static const int dbgWin = std::getenv("RDBG") ? std::atoi(std::getenv("RDBG")) : -1;
            if (winIdx == dbgWin && !car.dbgProbes.empty())
                std::printf("win %d f%d pos (%.2f %.2f %.2f) probes:%s\n", winIdx, i, car.pos.x, car.pos.y, car.pos.z, car.dbgProbes.c_str());
            const V3 f = car.rot.rotate({0, 0, 1}), l = car.rot.rotate({1, 0, 0}), u = car.rot.rotate({0, 1, 0});
            std::fprintf(out, "%.4f %.4f %.4f %.4f\n", dot(car.vel, f), dot(car.vel, l), dot(car.angVel, u), car.vel.y);
        }
    }
    std::fclose(in);
    std::fclose(out);
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 5 && std::string(argv[1]) == "--replay") return replay(argv[2], argv[3], argv[4], argc >= 6 ? argv[5] : nullptr);
    if (argc < 3) { std::printf("usage: physlab car.ep3 steer|wall|glance|slalom\n       physlab --probe track.ep3 x z\n"); return 1; }
    if (std::string(argv[1]) == "--near" && argc >= 7) {
        // list meshes with a triangle within r of (x,y,z): entity, material, render state, texture formats
        ep3::Package trk;
        if (!trk.load(argv[2])) return 1;
        const V3 c{float(std::atof(argv[3])), float(std::atof(argv[4])), float(std::atof(argv[5]))};
        const float r = float(std::atof(argv[6]));
        for (auto& e : trk.entities) {
            const float* M = e.world;
            for (auto& m : e.meshes) {
                bool hit = false;
                for (size_t v = 0; v + 2 < m.positions.size() && !hit; v += 3) {
                    float x = m.positions[v], y = m.positions[v + 1], z = m.positions[v + 2];
                    V3 p{x * M[0] + y * M[4] + z * M[8] + M[12], x * M[1] + y * M[5] + z * M[9] + M[13], x * M[2] + y * M[6] + z * M[10] + M[14]};
                    if (length(p - c) < r) hit = true;
                }
                if (!hit) continue;
                auto mit = trk.materials.find(m.material);
                std::string fm;
                if (mit != trk.materials.end())
                    for (auto& sl : mit->second.slots) {
                        auto t = trk.textures.find(sl.texture);
                        char b2[64]; std::snprintf(b2, sizeof b2, " %x:%02x:%s", sl.type, t != trk.textures.end() ? t->second.format : 0, t != trk.textures.end() ? (t->second.name.size() > 26 ? t->second.name.substr(t->second.name.size() - 26) : t->second.name).c_str() : "?");
                        fm += b2;
                    }
                std::printf("ent=%-30s mat=%d rs=%08x sh=%08x tris=%zu slots:%s\n", e.name.c_str(), m.material,
                            mit != trk.materials.end() ? mit->second.renderState : 0, mit != trk.materials.end() ? mit->second.shader : 0, m.indices.size() / 3, fm.c_str());
            }
        }
        return 0;
    }
    if (std::string(argv[1]) == "--probe" && argc >= 5) {
        // list every triangle under (x, z): height, material, render state, texture format
        ep3::Package trk;
        if (!trk.load(argv[2])) return 1;
        const float px = float(std::atof(argv[3])), pz = float(std::atof(argv[4]));
        for (auto& e : trk.entities) {
            const float* M = e.world;
            for (auto& m : e.meshes) {
                auto W = [&](uint32_t v) {
                    float x = m.positions[v * 3], y = m.positions[v * 3 + 1], z = m.positions[v * 3 + 2];
                    return V3{x * M[0] + y * M[4] + z * M[8] + M[12], x * M[1] + y * M[5] + z * M[9] + M[13], x * M[2] + y * M[6] + z * M[10] + M[14]};
                };
                for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                    V3 a = W(m.indices[i]), b = W(m.indices[i + 1]), c = W(m.indices[i + 2]);
                    auto side = [&](V3 p, V3 q) { return (q.x - p.x) * (pz - p.z) - (q.z - p.z) * (px - p.x); };
                    float s1 = side(a, b), s2 = side(b, c), s3 = side(c, a);
                    if (!((s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0))) continue;
                    V3 n = normalize(cross(b - a, c - a));
                    if (std::fabs(n.y) < 1e-3f) continue;
                    float y = a.y - (n.x * (px - a.x) + n.z * (pz - a.z)) / n.y;
                    auto mit = trk.materials.find(m.material);
                    uint32_t rs = 0, sh = 0; std::string fmts;
                    if (mit != trk.materials.end()) {
                        rs = mit->second.renderState; sh = mit->second.shader;
                        for (auto& s : mit->second.slots) {
                            auto t = trk.textures.find(s.texture);
                            char b2[48]; std::snprintf(b2, sizeof b2, " %x:%02x%s", s.type, t != trk.textures.end() ? t->second.format : 0, t != trk.textures.end() && t->second.cubemap ? "c" : "");
                            fmts += b2;
                        }
                    }
                    std::printf("y=%7.3f n.y=%+.2f ent=%-28s mat=%d rs=%08x sh=%08x part=%d slots:%s\n", y, n.y, e.name.c_str(), m.material, rs, sh, int(!m.particleCorner.empty()), fmts.c_str());
                }
            }
        }
        return 0;
    }
    const std::string carPath = argv[1], sc = argv[2];
    ep3::Package pkg;
    if (!pkg.load(carPath)) { std::printf("load failed\n"); return 1; }
    std::string phys = carPath.substr(0, carPath.size() - 4) + "_phys.xml";
    CarParams cp;
    if (!load_car_params(phys, cp)) { std::printf("phys failed %s\n", phys.c_str()); return 1; }
    CollisionWorld w;
    const float S = 600;
    w.addTriangle({-S, 0, -S}, {S, 0, -S}, {S, 0, S});
    w.addTriangle({-S, 0, -S}, {S, 0, S}, {-S, 0, S});
    // wall across +Z at z = 120 (perpendicular)  /  wall along a 25 degree line for glancing hits
    if (sc == "wallsteer" || sc == "wallrev") {
        w.addTriangle({-50, 0, 30}, {50, 0, 30}, {50, 1.0f, 30});
        w.addTriangle({-50, 0, 30}, {50, 1.0f, 30}, {-50, 1.0f, 30});
    }
    if (sc == "kerb") {
        const float hgt = std::getenv("KERB") ? float(std::atof(std::getenv("KERB"))) : 0.15f;
        w.addTriangle({4, hgt, -50}, {60, hgt, -50}, {60, hgt, 600});
        w.addTriangle({4, hgt, -50}, {60, hgt, 600}, {4, hgt, 600});
        w.addTriangle({4, 0, -50}, {4, 0, 600}, {4, hgt, 600});
        w.addTriangle({4, 0, -50}, {4, hgt, 600}, {4, hgt, -50});
    }
    if (sc == "wall") {
        w.addTriangle({-50, 0, 120}, {50, 0, 120}, {50, 3, 120});
        w.addTriangle({-50, 0, 120}, {50, 3, 120}, {-50, 3, 120});
    }
    if (sc == "glance") { // 15 degree wall coming in from the car's left
        V3 p0{2, 0, 50}, p1{2 - 150 * 0.268f, 0, 200};
        w.addTriangle(p0, p1, p1 + V3{0, 3, 0});
        w.addTriangle(p0, p1 + V3{0, 3, 0}, p0 + V3{0, 3, 0});
    }
    if (sc == "head") {
        const float a = 0.42f, z0 = 90;
        V3 p0{-10 * std::cos(a), 0, z0 - 10 * std::sin(a)}, p1{100 * std::cos(a), 0, z0 + 100 * std::sin(a)};
        // line through (0,z0) going towards +X,+Z ; car drives +Z at x = 3 and meets it
        p0 = {-60, 0, z0 - 60 * std::tan(a)}; p1 = {60, 0, z0 + 60 * std::tan(a)};
        w.addTriangle(p0, p1, p1 + V3{0, 3, 0});
        w.addTriangle(p0, p1 + V3{0, 3, 0}, p0 + V3{0, 3, 0});
    }
    w.build(4.0f);
    Vehicle car;
    car.setup(cp, pkg);
    car.reset({0, 0.1f, 0}, 0);
    const float dt = 1.0f / 60.0f;
    float t = 0, yaw = 0, maxY = 0;
    for (int f = 0; f < 60 * 14; ++f, t += dt) {
        CarInput in;
        in.throttle = 1;
        if (sc == "steer") {
            if (t > 4 && t < 5.5f) in.steer = 1;
            if (t > 9) in.throttle = 0;
        } else if (sc == "slalom") {
            if (t > 4) in.steer = std::fmod(t, 2.0f) < 1.0f ? 1.f : -1.f;
        } else if (sc == "brake") {          // original: 63 -> 14 km/h in 1.75 s, then reverse up to ~48 km/h
            static bool braking = false;
            if (car.speedKmh > 63.0f && !braking) braking = true;
            if (braking) { in.throttle = 0; in.brake = 1; }
        } else if (sc == "hbrake") {         // original: X+steer at 59 km/h -> 25 km/h after 1 s, yaw rate 31/56/64/68 at +0.1..0.4 s
            static float tH = -1;
            if (tH < 0 && car.speedKmh > 59.0f) tH = t;
            if (tH >= 0 && t - tH < 1.0f) { in.throttle = 0; in.handbrake = true; in.steer = 1; }
            if (tH >= 0 && t - tH >= 1.0f) { in.throttle = 0; }
        } else if (sc == "steer75") {        // original: full lock at 75 km/h -> yaw rate 21/90/108 deg/s after 0.1/0.3/0.5 s
            static float tSteer = -1;
            if (tSteer < 0 && car.speedKmh > 75.0f) tSteer = t;
            if (tSteer >= 0 && t - tSteer < 1.0f) in.steer = 1;
        } else if (sc == "kerb") {
            if (t > 3.0f) in.throttle = 0.6f;
            if (t > 3.0f && t < 3.6f) in.steer = 0.6f;
            if (t > 3.6f && t < 4.2f) in.steer = -0.6f;
        } else if (sc == "wallsteer") {
            if (t > 5) in.steer = 1;
        } else if (sc == "wallrev") {
            if (t > 5) { in.throttle = 0; in.brake = 1; in.steer = -1; }
            if (t > 7) { in.throttle = 1; in.brake = 0; in.steer = 1; }
        } else if (sc == "wall" || sc == "glance" || sc == "head") {
            if (t > 9) in.throttle = 0;
        }
        car.update(dt, in, w);
        const V3 fwd = car.rot.rotate({0, 0, 1}), upv = car.rot.rotate({0, 1, 0});
        yaw = std::atan2(fwd.x, fwd.z) * 57.3f;
        maxY = std::max(maxY, car.pos.y);
        if (car.impact > 0) { std::printf("IMPACT %.1f m/s at t=%.2f\n", car.impact, t); car.impact = 0; }
        if (f % 6 == 0)
            std::printf("t=%5.2f v=%5.1fkmh g=%d steer=%+.3f yaw=%+7.1f yawRate=%+6.1f pos=(%6.1f %5.2f %6.1f) vel=(%+5.1f %+5.1f %+5.1f) up.y=%.2f roll=%+5.1f contacts=%d%d%d%d\n",
                        t, car.speedKmh, car.gear, car.steerAngle(), yaw, car.angVel.y * 57.3f, car.pos.x, car.pos.y, car.pos.z,
                        car.vel.x, car.vel.y, car.vel.z, upv.y, std::asin(std::clamp(car.rot.rotate({1, 0, 0}).y, -1.0f, 1.0f)) * 57.3f, car.wheels[0].contact, car.wheels[1].contact, car.wheels[2].contact, car.wheels[3].contact);
        if (f % 6 == 0 && std::getenv("WDBG")) for (auto& wh : car.wheels) std::printf("     Fz=%.2f Fx=%+7.0f Fy=%+7.0f SR=%+6.1f SA=%+6.2f spin=%.1f\n", wh.dbgFz, wh.dbgFx, wh.dbgFy, wh.dbgSR, wh.dbgSA, wh.spinRate);
    }
    std::printf("maxY=%.2f\n", maxY);
    return 0;
}
