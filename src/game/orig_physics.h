#pragma once
// The original game's physics, run from the statically recompiled PPU code (src/recomp/full) on a memory
// snapshot of the user's own game taken in RPCS3 (tools/ps3snap.py). Every frame the player's raw pad inputs are
// written into the car object and the original World::update (0x31bab8) advances the whole physics world with its
// own fixed 1/60 s step. Bit-exact against the emulator (see docs / NOTAS).
#include "game/vehicle.h"
#include "game/vmath.h"
#include <cstdint>
#include <string>
#include <vector>

namespace gti {

class OrigPhysics {
public:
    // ebootElf: decrypted EBOOT; snap: guest memory snapshot. car/world = 0 -> found automatically.
    bool boot(const std::string& ebootElf, const std::string& snap, uint32_t car = 0, uint32_t world = 0);
    bool ok() const { return ok_; }
    void reset();                                   // back to the snapshot state
    // puts the car at rest with its origin (road level) at `origin`, facing `headingRad` (0 = +Z), e.g. a start locator
    void place(const V3& origin, float headingRad);
    void step(float dt, const CarInput& in);        // one rendered frame (any dt; the game sub-steps at 1/60 s)

    // state of the player's car after the last step (world space, same coordinates as the port's track)
    V3 origin, axisX, axisY, axisZ;                 // car model origin and axes (X left, Y up, Z forward)
    V3 com, vel, angVel;
    float rpm = 0, kmh = 0, steer = 0, throttle = 0;
    int gear = 1;
    // wheels as simulated by the original (index 0 FL, 1 FR, 2 RL, 3 RR, same as the port)
    struct Wheel {
        V3 hubLocal;          // wheel centre in the car frame (suspension travel included; x = tyre contact line)
        float steer = 0;      // steering angle, rad (+ = left)
        float spin = 0;       // rolling angle, rad (accumulated)
        float angSpeed = 0;   // rad/s
        float radius = 0.275f;
    };
    Wheel wheels[4];
    double stepMs = 0;                              // host time of the last step
    std::string error;
    uint32_t carAddr() const { return car_; }
    uint32_t worldAddr() const { return world_; }

private:
    bool ok_ = false;
    uint32_t car_ = 0, world_ = 0, frameDtAddr_ = 0;
    struct Region { uint32_t addr; std::vector<uint8_t> data; };
    std::vector<Region> snap_;
    void readState();
};

} // namespace gti
