#pragma once
// Car physics driven by the original <car>_phys.xml parameters (see docs/fase2_fisicas.md).
#include "formats/ep3.h"
#include "game/collision.h"
#include "game/vmath.h"
#include <array>
#include <string>
#include <vector>

namespace gti {

struct CarParams {
    // wheels (index 0 FL, 1 FR, 2 RL, 3 RR)
    float friction[4]{3.5f, 3.5f, 4.5f, 4.5f};
    float maxSteer = 0.39f, brakeTorque = 3100, handBrakeTorque = 4500, handBrakeInfluence = 0.5f, rollingDrag = 4;
    // suspension
    float springStrength = 30, springLength = 0.12f, springDamping = 3.4f, driveableExt = 0.1f;
    // generic
    float steerSpeed = 4, restoreSteerSpeed = 6.67f, pedalSpeed = 10, handbrakeSpeed = 5;
    float defaultAfterTouch = 0.2f, handbrakeAfterTouch = 0.5f;
    float airDrag = -8, driveRatio = 1, mass = 1140, dragConst = 0.05f, gravityMul = 1;
    V3 centerOfMass{0, 0.465f, 0.13f}, inertia{1.43f, 1.45f, 0.38f}, forceOffset{0, 0, 0};
    // engine
    std::vector<float> torque, rpm;
    float torqueMul = 1, clutchTimer = 0.15f, finalDrive = 2.74f;
    std::vector<float> gears; // [0] = reverse
    // tyres
    std::array<float, 15> lat{}, latHB{};
    std::array<float, 13> lon{}, lonHB{};
};

bool load_car_params(const std::string& physXmlPath, CarParams& out);

struct CarInput {
    float throttle = 0, brake = 0, steer = 0; // steer: -1 right .. +1 left (car +X = left)
    bool handbrake = false;
};

class Vehicle {
public:
    void setup(const CarParams& p, const ep3::Package& carPkg);
    void reset(const V3& position, float headingRad);
    void update(float dt, const CarInput& in, const CollisionWorld& world);
    // replay/testing: impose a driving state on a car that is resting on the ground (keeps pos/orientation)
    void setPose(const V3& origin, const V3& axisX, const V3& axisY, const V3& axisZ);
    void setDrivingState(float forwardSpeed, float lateralSpeed, float yawRate, int gearIdx, float steerN, float throttle, float brake);

    // state (COM in world space)
    V3 pos, vel, angVel;
    Quat rot;
    float rpm = 1000, speedKmh = 0;
    int gear = 1; // 0 = reverse, 1.. forward
    struct Wheel {
        V3 hardpoint;        // car local (relative to car origin)
        float radius = 0.27f;
        float steer = 0, spin = 0, spinRate = 0, compression = 0, prevCompression = 0;
        bool contact = false, driven = false, front = false;
        V3 hubLocal;         // render hub position, car local
        float dbgFx = 0, dbgFy = 0, dbgFz = 0, dbgSR = 0, dbgSA = 0;
    };
    std::array<Wheel, 4> wheels;
    // car-origin world transform (for rendering)
    V3 originWorld() const { return pos - rot.rotate(params.centerOfMass); }
    CarParams params;
    float steerAngle() const { return steerState_; }
    std::string dbgProbes;  // chassis contacts of the last step (debug)
    float impact = 0;  // strongest wall hit (m/s into the surface) since the caller last cleared it
    float scrape = 0;  // sliding speed along a wall while touching it (m/s, smoothed)

private:
    float flipTimer_ = 0, steerState_ = 0, throttleState_ = 0, brakeState_ = 0, shiftTimer_ = 0, acc_ = 0;
    void step(float dt, const CarInput& in, const CollisionWorld& world);
};

} // namespace gti
