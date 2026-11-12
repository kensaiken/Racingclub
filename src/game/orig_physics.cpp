#include "game/orig_physics.h"
#include "recomp/guest.h"
#include "recomp/hle.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gti::recomp { extern uint64_t g_timebaseFixed; }

namespace gti {

using namespace gti::recomp;

namespace {

Guest g_guest;
bool g_guestInit = false;

// lv2 system calls reached from the physics frame (memory, tty); everything else answers CELL_OK
void sys_stub(PPU& c) {
    if (c.r[11] == 348) {   // sys_memory_allocate(size, flags, u32* addr)
        static uint32_t next = 0x70000000;
        const uint32_t size = uint32_t(c.r[3]);
        const uint32_t align = (c.r[4] & 0x400) ? 0x100000 : 0x10000;
        next = (next + align - 1) & ~(align - 1);
        c.wr32(uint32_t(c.r[5]), next);
        next += size;
    }
    if (c.r[11] == 352) {   // sys_memory_get_user_memory_size
        c.wr32(uint32_t(c.r[3]), 0x0D000000);
        c.wr32(uint32_t(c.r[3]) + 4, 0x04000000);
    }
    c.r[3] = 0;
}

constexpr uint32_t kWorldVtable = 0x00bfbaa0;   // physics world (World::update = 0x31bab8)
constexpr uint32_t kCarVtable = 0x00bfc0e0;     // car object
constexpr uint32_t kWorldUpdate = 0x31bab8;

uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
float bef(const uint8_t* p) { uint32_t u = be32(p); float f; std::memcpy(&f, &u, 4); return f; }

} // namespace

bool OrigPhysics::boot(const std::string& ebootElf, const std::string& snapPath, uint32_t car, uint32_t world) {
    ok_ = false;
    if (!g_guestInit) {
        g_guest.stackTop = 0xEF000000;   // away from the snapshot's own thread stacks
        if (!g_guest.init(ebootElf)) { error = "no se pudo cargar " + ebootElf; return false; }
        hle_install();
        g_syscallHook = sys_stub;
        g_timebaseFixed = 100;            // deterministic time base: same run every time
        g_guestInit = true;
    }
    // keep the snapshot regions (for reset)
    snap_.clear();
    FILE* f = std::fopen(snapPath.c_str(), "rb");
    if (!f) { error = "no existe " + snapPath; return false; }
    char magic[4];
    uint32_t n = 0;
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "PS3S", 4) != 0 || std::fread(&n, 4, 1, f) != 1) { std::fclose(f); error = "instantanea no valida"; return false; }
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t hdr[2];
        if (std::fread(hdr, 4, 2, f) != 2) break;
        Region r;
        r.addr = hdr[0];
        r.data.resize(hdr[1]);
        if (std::fread(r.data.data(), 1, hdr[1], f) != hdr[1]) break;
        snap_.push_back(std::move(r));
    }
    std::fclose(f);
    // locate the physics world and the player's car when not given
    std::vector<uint32_t> worlds;
    if (world) worlds.push_back(world);
    else
        for (auto& r : snap_) {
            const uint8_t* d = r.data.data();
            for (size_t o = 0; o + 0x100 < r.data.size(); o += 16)
                if (be32(d + o) == kWorldVtable && std::fabs(bef(d + o + 0x9c) - 1.0f / 60.0f) < 1e-4f) worlds.push_back(r.addr + uint32_t(o));
        }
    reset();
    PPU c;
    g_guest.prepare(c);
    // the live car is a body of the live physics world (stale objects of earlier sessions stay in memory)
    world = 0;
    for (uint32_t w : worlds) {
        const uint32_t list = c.rd32(w + 0xc4), n = c.rd32(w + 0xc8);
        if (!list || n > 4096) continue;
        uint32_t found = 0;
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t b = c.rd32(list + 4 * k);
            if (b && b < 0xf0000000u && c.rd32(b) == kCarVtable) found = b;   // the player's car is added last
        }
        if (found && (!car || car == found)) { world = w; car = found; break; }
        if (!found && car) world = w;
    }
    if (!car || !world) { error = "no se encontro el coche o el mundo fisico en la instantanea"; return false; }
    car_ = car;
    world_ = world;
    // frame time the game hands to World::update: *(*(*(toc+0x25f4))+0xcc)+0xbc
    const uint32_t g0 = c.rd32(c.rd32(g_guest.toc + 0x25f4));
    frameDtAddr_ = c.rd32(g0 + 0xcc) + 0xbc;
    ok_ = true;
    readState();
    return true;
}

void OrigPhysics::reset() {
    for (auto& r : snap_) std::memcpy(g_guest.mem + r.addr, r.data.data(), r.data.size());
}

void OrigPhysics::place(const V3& o, float h) {
    if (!ok_) return;
    PPU c;
    g_guest.prepare(c);
    auto wv = [&](uint32_t a, const V3& v) { c.wrf32(a, v.x); c.wrf32(a + 4, v.y); c.wrf32(a + 8, v.z); };
    auto rv = [&](uint32_t a) { return V3{c.rdf32(a), c.rdf32(a + 4), c.rdf32(a + 8)}; };
    // centre of mass in the car frame, from the current state
    const V3 X0 = rv(car_ + 0x20), Y0 = rv(car_ + 0x30), Z0 = rv(car_ + 0x40), r = rv(car_ + 0x90) - rv(car_ + 0x50);
    const V3 comL{dot(r, X0), dot(r, Y0), dot(r, Z0)};
    const V3 X{std::cos(h), 0, -std::sin(h)}, Y{0, 1, 0}, Z{std::sin(h), 0, std::cos(h)};
    const V3 com = o + X * comL.x + Y * comL.y + Z * comL.z;
    // orientation rows: current (+0x20), copy (+0x60), predicted (+0x100), previous step (+0x190)
    for (uint32_t blk : {0x20u, 0x60u, 0x100u, 0x190u}) { wv(car_ + blk, X); wv(car_ + blk + 0x10, Y); wv(car_ + blk + 0x20, Z); }
    wv(car_ + 0x50, o);       // origin
    wv(car_ + 0x90, com);     // centre of mass
    wv(car_ + 0xf0, com);     // predicted centre of mass
    wv(car_ + 0x180, com);    // previous step
    wv(car_ + 0x140, V3{});   // velocity
    wv(car_ + 0x150, V3{});   // angular velocity
    readState();
}

void OrigPhysics::step(float dt, const CarInput& in) {
    if (!ok_) return;
    PPU c;
    g_guest.prepare(c);
    // raw pad inputs of the player (the game smooths them itself): throttle, brake, steer (+ = left), handbrake
    c.wrf32(car_ + 0x410, in.throttle);
    c.wrf32(car_ + 0x418, in.brake);
    c.wrf32(car_ + 0x420, in.steer);
    c.wrf32(car_ + 0x41c, in.handbrake ? 1.0f : 0.0f);
    c.wrf32(car_ + 0x424, in.brake > 0.5f ? 1.0f : 0.0f);   // brake button (verified against the emulator recording)
    c.wrf32(frameDtAddr_, dt);
    const auto t0 = std::chrono::steady_clock::now();
    c.r[3] = world_;
    c.f[1] = dt;
    rc_call(c, kWorldUpdate);
    // engine / gearbox model (rpm, automatic shifts) of the car: 0x26b078(engine, dt); feeds sound and HUD only,
    // the physics world above already contains the drivetrain
    if (!c.missing && !c.unimplAt) {
        const uint32_t eng = c.rd32(car_ + 0xb4);
        g_guest.prepare(c);
        c.r[3] = eng;
        c.f[1] = dt;
        rc_call(c, 0x26b078);
    }
    stepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (c.missing || c.unimplAt) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "codigo original no traducido: %s 0x%x", c.missing ? "llamada" : "instruccion", c.missing ? c.missing : c.unimplAt);
        error = buf;
        ok_ = false;
    }
    readState();
}

void OrigPhysics::readState() {
    PPU c;
    g_guest.prepare(c);
    auto v3 = [&](uint32_t a) { return V3{c.rdf32(a), c.rdf32(a + 4), c.rdf32(a + 8)}; };
    axisX = v3(car_ + 0x20);
    axisY = v3(car_ + 0x30);
    axisZ = v3(car_ + 0x40);
    origin = v3(car_ + 0x50);
    com = v3(car_ + 0x90);
    vel = v3(car_ + 0x140);
    angVel = v3(car_ + 0x150);
    throttle = c.rdf32(car_ + 0x3f4);
    steer = c.rdf32(car_ + 0x404);
    const uint32_t eng = c.rd32(car_ + 0xb4);
    rpm = c.rdf32(eng + 0x1ac);
    gear = int(c.rd32(eng + 0x1a4));
    kmh = length(vel) * 3.6f;
    // wheel objects (car+0x2c8.. ): radius +0x4, rolling angle +0xc, steering angle +0x18, angular speed +0x24,
    // tyre contact point in world space +0x50 (end of the suspension ray; wheel centre = contact + radius)
    for (int i = 0; i < 4; ++i) {
        const uint32_t w = c.rd32(car_ + 0x2c8 + 4 * i);
        auto& o = wheels[i];
        o.radius = c.rdf32(w + 0x4);
        o.spin = c.rdf32(w + 0xc);
        o.steer = c.rdf32(w + 0x18);
        o.angSpeed = c.rdf32(w + 0x24);
        const V3 r = v3(w + 0x50) - origin;
        o.hubLocal = {dot(r, axisX), dot(r, axisY) + o.radius, dot(r, axisZ)};
    }
}

} // namespace gti
