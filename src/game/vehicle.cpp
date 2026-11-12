#include "game/vehicle.h"
#include "util/bytes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gti {

namespace {

constexpr float kPi = 3.14159265358979f;

std::vector<float> xml_values(const std::string& xml, const std::string& tag) {
    std::vector<float> out;
    const std::string open = "<" + tag + ">";
    size_t s = xml.find(open);
    if (s == std::string::npos) return out;
    s += open.size();
    size_t e = xml.find("</" + tag + ">", s);
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
float xml_value(const std::string& xml, const std::string& tag, float def) {
    auto v = xml_values(xml, tag);
    return v.empty() ? def : v[0];
}
int xml_int(const std::string& xml, const std::string& tag, int def) {  // hex-formatted ints like 00000009
    const std::string open = "<" + tag + ">";
    size_t s = xml.find(open);
    if (s == std::string::npos) return def;
    return int(std::strtol(xml.c_str() + s + open.size(), nullptr, 16));
}

// Pacejka '94 (Fz in kN, x = slip angle in degrees or slip ratio in percent)
float magic(float x, float B, float C, float D, float E) {
    float bx = B * x;
    return D * std::sin(C * std::atan(bx - E * (bx - std::atan(bx))));
}
float pacejka_lat(const std::array<float, 15>& a, float alphaDeg, float Fz, float* peak) {
    const float C = a[0];
    const float D = Fz * (a[1] * Fz + a[2]);
    const float BCD = a[3] * std::sin(2.0f * std::atan(Fz / a[4]));
    const float B = (C * D) != 0 ? BCD / (C * D) : 0;
    const float E = a[6] * Fz + a[7];
    const float Sh = a[9] * Fz + a[10];
    const float Sv = (a[12] * Fz + a[13]) * Fz;
    if (peak) *peak = std::fabs(D);
    (void)Sh; (void)Sv; // shifts are side-specific in the original; ignoring them keeps the car symmetric
    return magic(alphaDeg, B, C, D, E);
}
float pacejka_lon(const std::array<float, 13>& b, float slipPct, float Fz, float* peak) {
    const float C = b[0];
    const float D = Fz * (b[1] * Fz + b[2]);
    const float BCD = (b[3] * Fz * Fz + b[4] * Fz) * std::exp(-b[5] * Fz);
    const float B = (C * D) != 0 ? BCD / (C * D) : 0;
    const float E = b[6] * Fz * Fz + b[7] * Fz + b[8];
    const float Sh = b[9] * Fz + b[10];
    const float Sv = b[11] * Fz + b[12];
    if (peak) *peak = std::fabs(D);
    (void)Sh; (void)Sv;
    return magic(slipPct, B, C, D, E);
}

float approach(float cur, float target, float rate) {
    if (cur < target) return std::min(target, cur + rate);
    return std::max(target, cur - rate);
}

} // namespace

bool load_car_params(const std::string& path, CarParams& p) {
    std::vector<uint8_t> raw;
    if (!read_file(path, raw)) return false;
    std::string x(raw.begin(), raw.end());
    p.friction[0] = xml_value(x, "Wheel_FrictionFrontLeftSide", p.friction[0]);
    p.friction[1] = xml_value(x, "Wheel_FrictionFrontRightSide", p.friction[1]);
    p.friction[2] = xml_value(x, "Wheel_FrictionRearLeftSide", p.friction[2]);
    p.friction[3] = xml_value(x, "Wheel_FrictionRearRightSide", p.friction[3]);
    p.maxSteer = xml_value(x, "Wheel_MaxSteerAngleLeft", p.maxSteer);
    p.brakeTorque = xml_value(x, "Wheel_BrakeTorque", p.brakeTorque);
    p.handBrakeTorque = xml_value(x, "Wheel_HandBrakeTorque", p.handBrakeTorque);
    p.handBrakeInfluence = xml_value(x, "Wheel_HandBrakeInfluence", p.handBrakeInfluence);
    p.rollingDrag = xml_value(x, "Wheel_RollingDrag", p.rollingDrag);
    p.springStrength = xml_value(x, "Susp_SpringStrength", p.springStrength);
    p.springLength = xml_value(x, "Susp_SpringLength", p.springLength);
    p.springDamping = xml_value(x, "Susp_SpringDamping", p.springDamping);
    p.driveableExt = xml_value(x, "Susp_DriveableExt", p.driveableExt);
    p.steerSpeed = xml_value(x, "Gen_SteerSpeed", p.steerSpeed);
    p.restoreSteerSpeed = xml_value(x, "Gen_RestoreSteerSpeed", p.restoreSteerSpeed);
    p.pedalSpeed = xml_value(x, "Gen_PedalSpeed", p.pedalSpeed);
    p.handbrakeSpeed = xml_value(x, "Gen_HandbrakeSpeed", p.handbrakeSpeed);
    p.defaultAfterTouch = xml_value(x, "Gen_DefaultAfterTouch", p.defaultAfterTouch);
    p.handbrakeAfterTouch = xml_value(x, "Gen_HandbrakeAfterTouch", p.handbrakeAfterTouch);
    p.airDrag = xml_value(x, "Gen_AirDrag", p.airDrag);
    p.driveRatio = xml_value(x, "Gen_DriveRatio", p.driveRatio);
    p.mass = xml_value(x, "Gen_CarMass", p.mass);
    p.dragConst = xml_value(x, "Gen_DragConst", p.dragConst);
    p.gravityMul = xml_value(x, "Gen_GravityMul", p.gravityMul);
    if (auto v = xml_values(x, "Gen_CenterOfMass"); v.size() >= 3) p.centerOfMass = {v[0], v[1], v[2]};
    if (auto v = xml_values(x, "Gen_Inertia"); v.size() >= 3) p.inertia = {v[0], v[1], v[2]};
    if (auto v = xml_values(x, "Force_Offset"); v.size() >= 3) p.forceOffset = {v[0], v[1], v[2]};
    const int ntc = xml_int(x, "TC_Num_Points", 0);
    p.torque.clear(); p.rpm.clear();
    for (int i = 0; i < ntc; ++i) {
        p.torque.push_back(xml_value(x, "TC_Torque_Point_" + std::to_string(i), 0));
        p.rpm.push_back(xml_value(x, "TC_RPM_Point_" + std::to_string(i), 0));
    }
    p.torqueMul = xml_value(x, "TC_Torque_Mul", p.torqueMul);
    p.clutchTimer = xml_value(x, "Gear_Clutch_Timer", p.clutchTimer);
    p.finalDrive = xml_value(x, "Gear_Final_Drive", p.finalDrive);
    const int ng = xml_int(x, "Gear_Num", 0);
    p.gears.clear();
    for (int i = 0; i < ng; ++i) p.gears.push_back(xml_value(x, "Gear_Ratio_" + std::to_string(i), 1));
    auto fill = [&](const char* tag, auto& arr) {
        auto v = xml_values(x, tag);
        for (size_t i = 0; i < arr.size() && i < v.size(); ++i) arr[i] = v[i];
    };
    fill("Pacejka_Lat", p.lat); fill("Pacejka_Long", p.lon);
    fill("Pacejka_Lat_Handbrake", p.latHB); fill("Pacejka_Long_Handbrake", p.lonHB);
    return !p.torque.empty() && p.gears.size() >= 2;
}

void Vehicle::setup(const CarParams& p, const ep3::Package& pkg) {
    params = p;
    static const char* hp[4] = {"hardPointFL", "hardPointFR", "hardPointRL", "hardPointRR"};
    static const char* wn[4] = {"WheelFL", "WheelFR", "WheelRL", "WheelRR"};
    static const V3 defHp[4] = {{0.65f, 0.3f, 1.2f}, {-0.65f, 0.3f, 1.2f}, {0.65f, 0.3f, -1.1f}, {-0.65f, 0.3f, -1.1f}};
    for (int i = 0; i < 4; ++i) {
        Wheel& w = wheels[i];
        w = Wheel{};
        w.front = i < 2;
        const ep3::Locator* l = pkg.findLocator(hp[i]);
        w.hardpoint = l ? V3{l->matrix[12], l->matrix[13], l->matrix[14]} : defHp[i];
        w.hubLocal = {w.hardpoint.x, w.hardpoint.y - 0.03f, w.hardpoint.z};
        for (auto& e : pkg.entities) {
            if (e.name != wn[i]) continue;
            w.hubLocal = {e.world[12], e.world[13], e.world[14]};
            float ymin = 1e9f, ymax = -1e9f;
            for (auto& m : e.meshes)
                for (size_t v = 1; v < m.positions.size(); v += 3) { ymin = std::min(ymin, m.positions[v]); ymax = std::max(ymax, m.positions[v]); }
            if (ymax > ymin) w.radius = 0.5f * (ymax - ymin);
        }
    }
    // the art's hardpoints are a few mm off between left and right; make each axle symmetric so the car runs straight
    for (int a = 0; a < 4; a += 2) {
        Wheel& l = wheels[a]; Wheel& r = wheels[a + 1];
        const float hy = 0.5f * (l.hardpoint.y + r.hardpoint.y), hz = 0.5f * (l.hardpoint.z + r.hardpoint.z);
        const float hx = 0.5f * (std::fabs(l.hardpoint.x) + std::fabs(r.hardpoint.x));
        l.hardpoint = {hx, hy, hz}; r.hardpoint = {-hx, hy, hz};
        const float rad = 0.5f * (l.radius + r.radius);
        l.radius = r.radius = rad;
    }
    const float frontShare = std::clamp(params.driveRatio, 0.0f, 1.0f);
    for (int i = 0; i < 4; ++i) wheels[i].driven = wheels[i].front ? frontShare > 0.01f : frontShare < 0.99f;
}

void Vehicle::reset(const V3& position, float heading) {
    rot = Quat::axisAngle({0, 1, 0}, heading);
    pos = position + rot.rotate(params.centerOfMass) + V3{0, 0.25f, 0};
    vel = angVel = {0, 0, 0};
    gear = 1; rpm = 1000; steerState_ = throttleState_ = brakeState_ = shiftTimer_ = 0;
    for (auto& w : wheels) { w.spinRate = 0; w.compression = w.prevCompression = 0; w.steer = 0; }
}

void Vehicle::setPose(const V3& origin, const V3& X, const V3& Y, const V3& Z) {
    // rotation whose columns are the car axes (X = left, Y = up, Z = forward) -> quaternion
    const float m00 = X.x, m01 = Y.x, m02 = Z.x, m10 = X.y, m11 = Y.y, m12 = Z.y, m20 = X.z, m21 = Y.z, m22 = Z.z;
    const float tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) { float s = std::sqrt(tr + 1.0f) * 2; q.w = 0.25f * s; q.x = (m21 - m12) / s; q.y = (m02 - m20) / s; q.z = (m10 - m01) / s; }
    else if (m00 > m11 && m00 > m22) { float s = std::sqrt(1.0f + m00 - m11 - m22) * 2; q.w = (m21 - m12) / s; q.x = 0.25f * s; q.y = (m01 + m10) / s; q.z = (m02 + m20) / s; }
    else if (m11 > m22) { float s = std::sqrt(1.0f + m11 - m00 - m22) * 2; q.w = (m02 - m20) / s; q.x = (m01 + m10) / s; q.y = 0.25f * s; q.z = (m12 + m21) / s; }
    else { float s = std::sqrt(1.0f + m22 - m00 - m11) * 2; q.w = (m10 - m01) / s; q.x = (m02 + m20) / s; q.y = (m12 + m21) / s; q.z = 0.25f * s; }
    rot = q;
    rot.normalize();
    pos = origin + rot.rotate(params.centerOfMass);
    for (auto& w : wheels) { w.compression = w.prevCompression = 0.08f; w.contact = true; }
}

void Vehicle::setDrivingState(float vfwd, float vlat, float yawRate, int gearIdx, float steerN, float throttle, float brake) {
    const V3 fwd = rot.rotate({0, 0, 1}), left = rot.rotate({1, 0, 0}), up = rot.rotate({0, 1, 0});
    vel = fwd * vfwd + left * vlat;
    angVel = up * yawRate;
    for (auto& w : wheels) w.spinRate = vfwd / w.radius;
    gear = gearIdx;
    shiftTimer_ = 0;
    steerState_ = steerN * params.maxSteer;
    throttleState_ = throttle;
    brakeState_ = brake;
    acc_ = 0;
}

void Vehicle::update(float dt, const CarInput& in, const CollisionWorld& world) {
    const float h = 1.0f / 240.0f;
    acc_ += std::min(dt, 0.1f);
    while (acc_ >= h) { step(h, in, world); acc_ -= h; }
    for (auto& w : wheels) w.spin += w.spinRate * dt;
}

void Vehicle::step(float dt, const CarInput& in, const CollisionWorld& world) {
    const CarParams& P = params;
    const V3 up = rot.rotate({0, 1, 0}), fwd = rot.rotate({0, 0, 1}), left = rot.rotate({1, 0, 0});
    const float vf = dot(vel, fwd);
    const float speed = length(vel);
    speedKmh = std::fabs(vf) * 3.6f;

    // ---- driver inputs (smoothed like the original Gen_*Speed parameters) ----
    const float steerLimit = P.maxSteer;
    const float steerTarget = std::clamp(in.steer, -1.0f, 1.0f) * steerLimit;
    const float steerRate = (std::fabs(steerTarget) < std::fabs(steerState_) ? P.restoreSteerSpeed : P.steerSpeed) * P.maxSteer * dt;
    steerState_ = approach(steerState_, steerTarget, steerRate);
    float throttleIn = in.throttle, brakeIn = in.brake;
    // reverse: brake while (almost) stopped engages reverse, throttle returns to 1st
    if (gear != 0 && brakeIn > 0.5f && throttleIn < 0.1f && vf < 0.5f) gear = 0;
    if (gear == 0 && throttleIn > 0.1f && vf > -0.5f) gear = 1;
    if (gear == 0) std::swap(throttleIn, brakeIn);
    throttleState_ = approach(throttleState_, throttleIn, P.pedalSpeed * dt);
    brakeState_ = approach(brakeState_, brakeIn, P.pedalSpeed * dt);

    // ---- engine & automatic gearbox ----
    const int ng = int(P.gears.size());
    const float maxRpm = P.rpm.empty() ? 6500.f : P.rpm.back();
    float drivenSpin = 0; int nd = 0;
    // engine speed follows the driven wheels that touch the ground (a wheel spinning in the air doesn't drag the revs up)
    int ndGround = 0; float groundSpin = 0;
    for (auto& w : wheels) if (w.driven) { drivenSpin += w.spinRate; ++nd; if (w.contact) { groundSpin += w.spinRate; ++ndGround; } }
    if (ndGround) { drivenSpin = groundSpin; nd = ndGround; }
    drivenSpin = nd ? drivenSpin / nd : 0;
    const float ratio = P.gears.empty() ? 1.f : P.gears[std::clamp(gear, 0, ng - 1)];
    static const float kDriveR = std::getenv("DRIVER") ? float(std::atof(std::getenv("DRIVER"))) : 1.279f;
    rpm = std::clamp(std::fabs(drivenSpin * ratio * P.finalDrive / kDriveR) * 60.0f / (2 * kPi), 1000.0f, maxRpm + 400.0f);
    if (shiftTimer_ > 0) shiftTimer_ -= dt;
    else if (gear >= 1) {
        static const float kShiftT = std::getenv("SHIFTT") ? float(std::atof(std::getenv("SHIFTT"))) : -1.0f;
        if (rpm > maxRpm * 0.985f && gear < ng - 1) { ++gear; shiftTimer_ = kShiftT >= 0 ? kShiftT : P.clutchTimer; }
        else if (gear > 1 && rpm < maxRpm * 0.38f && rpm * P.gears[gear - 1] / P.gears[gear] < maxRpm * 0.75f) { --gear; shiftTimer_ = P.clutchTimer; }
    }
    float engineTorque = 0;
    if (!P.torque.empty()) {
        size_t k = 0;
        while (k + 1 < P.rpm.size() && P.rpm[k + 1] < rpm) ++k;
        float t = P.torque[k];
        if (k + 1 < P.rpm.size()) {
            float f = std::clamp((rpm - P.rpm[k]) / std::max(1.0f, P.rpm[k + 1] - P.rpm[k]), 0.0f, 1.0f);
            t = P.torque[k] + (P.torque[k + 1] - P.torque[k]) * f;
        }
        // limiter; in reverse the original tops out at ~48 km/h (Mini), i.e. ~0.92 of the forward limiter rpm
        static const float kRevLim = std::getenv("REVLIM") ? float(std::atof(std::getenv("REVLIM"))) : 0.92f;
        const float lim = gear == 0 ? maxRpm * kRevLim : maxRpm + 200;
        engineTorque = rpm > lim ? 0 : t * P.torqueMul * throttleState_;
        // engine braking when lifting off
        const float maxT = *std::max_element(P.torque.begin(), P.torque.end()) * P.torqueMul;
        // the original coasts with almost no engine braking (speed holds when lifting off)
        static const float kEngB = std::getenv("ENGB") ? float(std::atof(std::getenv("ENGB"))) : 0.0f;
        if (rpm > 1100) engineTorque -= (1.0f - throttleState_) * kEngB * maxT * (rpm / maxRpm);
    }
    if (shiftTimer_ > 0) engineTorque = 0;
    const float wheelTorqueTotal = engineTorque * ratio * P.finalDrive;
    const float frontShare = std::clamp(P.driveRatio, 0.0f, 1.0f);

    // ---- forces ----
    // measured in the original: a car in the air falls at ~2 g (per-frame dv = -0.297 m/s at 60 Hz incl. drag)
    static const float kGrav = std::getenv("GRAVK") ? float(std::atof(std::getenv("GRAVK"))) : 2.0f;
    // GRAVMODE 0: extra gravity always; 1: extra gravity only while no wheel touches the ground (slopes feel 1 g)
    static const int kGravMode = std::getenv("GRAVMODE") ? std::atoi(std::getenv("GRAVMODE")) : 0;
    bool anyContact = false;
    for (auto& w : wheels) anyContact |= w.contact;
    const float gNow = (kGravMode == 1 && anyContact) ? 1.0f : kGrav;
    const float springScale = kGravMode == 1 ? 1.0f : kGrav;
    V3 F{0, -9.81f * P.gravityMul * gNow * P.mass, 0}, T{0, 0, 0};
    const V3 origin = pos - rot.rotate(P.centerOfMass);
    static const float kDampK = std::getenv("DAMPK") ? float(std::atof(std::getenv("DAMPK"))) : 1.0f;
    const float k = P.springStrength * P.mass * springScale, c = P.springDamping * P.mass * std::sqrt(springScale) * kDampK;
    const float baseWheelInertia = 1.2f;
    // engine + gearbox inertia reflected to the driven wheels: I_engine * (gear * final)^2 shared by the driven wheels
    static const float engineInertia = std::getenv("ENGI") ? float(std::atof(std::getenv("ENGI"))) : 0.0f;
    const float gearing = ratio * P.finalDrive;
    const float reflected = shiftTimer_ > 0 ? 0.0f : engineInertia * gearing * gearing / float(std::max(nd, 1));
    for (int i = 0; i < 4; ++i) {
        Wheel& w = wheels[i];
        w.steer = w.front ? steerState_ : 0;
        const V3 hp = origin + rot.rotate(w.hardpoint);
        const float rayLen = P.springLength + w.radius + P.driveableExt;
        RayHit hit;
        w.prevCompression = w.compression;
        // cast from above the hardpoint so a wheel wedged against a raised kerb/median still finds its top
        const float lift = 0.45f;
        w.contact = world.raycast(hp + up * lift, -up, rayLen + lift, hit) && hit.normal.y > 0.2f;
        hit.t -= lift;
        const float wheelInertia = baseWheelInertia + (w.driven ? reflected : 0.0f);
        float driveT = 0;
        if (w.driven && nd) {
            const float share = w.front ? frontShare : 1.0f - frontShare;
            // limited-slip: the axle's torque goes to its wheels that have ground contact (previous step)
            const Wheel& other = wheels[i ^ 1];
            const int nSame = (w.contact && other.contact) || (!w.contact && !other.contact) ? 2 : 1;
            (void)nSame;
            // original: drive force in 1st and 2nd is grip-limited (both ~13-15 m/s2 on the Mini), i.e. the engine
            // delivers more torque at the wheel than the rpm coupling alone implies
            static const float kTq = std::getenv("TQK") ? float(std::atof(std::getenv("TQK"))) : 0.825f;
            // per-gear force factors fitted to the original's measured full-throttle acceleration curve
            static float kTqG[8] = {0};
            if (kTqG[0] == 0) {
                const float def[8] = {1.0f, 1.433f, 1.312f, 1.154f, 1.062f, 1.0f, 1.0f, 1.0f};
                for (int g = 0; g < 8; ++g) {
                    char nm[8]; std::snprintf(nm, sizeof nm, "TQG%d", g);
                    kTqG[g] = std::getenv(nm) ? float(std::atof(std::getenv(nm))) : def[g];
                }
            }
            const float gearK = kTqG[std::clamp(gear, 0, 7)];
            driveT = (!w.contact && other.contact) ? 0.0f : wheelTorqueTotal * share * kTq * gearK / kDriveR;
        }
        const bool hb = in.handbrake && !w.front;
        // service brake scaled to the measured original deceleration (~7.8 m/s2 for the Mini, wheels not locking)
        static const float kBrakeK = std::getenv("BRAKEK") ? float(std::atof(std::getenv("BRAKEK"))) : 0.26f;
        static const float kHbK = std::getenv("HBK") ? float(std::atof(std::getenv("HBK"))) : 0.3022f;
        const float brakeT = P.brakeTorque * kBrakeK * brakeState_ * (w.front ? 1.0f : 0.6f) + (hb ? P.handBrakeTorque * kHbK : 0.0f);
        if (!w.contact) {
            w.compression = 0;
            w.spinRate += driveT / wheelInertia * dt;
            w.spinRate = approach(w.spinRate, 0, (brakeT / wheelInertia + 2.0f) * dt);
            continue;
        }
        w.compression = std::clamp(P.springLength + w.radius - hit.t, 0.0f, P.springLength + 0.1f);
        // the wheel rides up a kerb over a few steps instead of teleporting onto it
        w.compression = std::min(w.compression, w.prevCompression + 1.5f * dt);
        // a wheel climbing a kerb changes compression in a single step: limit the damper speed and the
        // total wheel load so the car rides up instead of being launched
        const float compVel = std::clamp((w.compression - w.prevCompression) / dt, -2.5f, 2.5f);
        float Fs = k * w.compression + c * compVel;
        if (w.compression > P.springLength) Fs += k * 4.0f * (w.compression - P.springLength); // bump stop
        Fs = std::min(Fs, P.mass * 9.81f * kGrav * 1.6f * std::max(1.0f, kDampK));
        Fs = std::max(0.0f, Fs);
        const V3 n = hit.normal;
        V3 wf = fwd * std::cos(w.steer) + left * std::sin(w.steer);
        wf = normalize(wf - n * dot(wf, n));
        const V3 ws = cross(n, wf);
        const V3 P0 = hit.point;
        const V3 vc = vel + cross(angVel, P0 - pos);
        const float vl = dot(vc, wf), vs = dot(vc, ws);
        const float denom = std::max(std::fabs(vl), 2.0f);
        const float slipRatio = (w.spinRate * w.radius - vl) / denom * 100.0f;
        const float slipAngle = std::atan2(vs, denom) * 180.0f / kPi;
        (void)slipRatio;
        const float Fz = std::clamp(Fs / 1000.0f, 0.0f, 12.0f);
        static const float kGripDiv = std::getenv("GRIPDIV") ? float(std::atof(std::getenv("GRIPDIV"))) : 1.65f;
        const float grip = P.friction[i] / kGripDiv;
        const auto& lonP = hb ? P.lonHB : P.lon;
        auto fxOf = [&](float spin, float* pk) {
            const float sr = (spin * w.radius - vl) / denom * 100.0f;
            return pacejka_lon(lonP, sr, Fz, pk) * grip;
        };
        float peakY = 1;
        static const float kLatStiff = std::getenv("LATK") ? float(std::atof(std::getenv("LATK"))) : 1.81f;
        float Fy = (hb ? pacejka_lat(P.latHB, slipAngle * kLatStiff, Fz, &peakY) : pacejka_lat(P.lat, slipAngle * kLatStiff, Fz, &peakY)) * grip;
        peakY *= grip;
        // ---- wheel spin: brakes first (never reversing the wheel), then an implicit tyre/drive solve
        // handbrake: a (scaled) torque on the rear wheels with the handbrake tyre curves, like the original
        float peakX = 1;
        {
            const float s0 = w.spinRate, fx0 = fxOf(s0, &peakX);
            const float eps = 0.05f;
            // dFx/dspin: tangent, but never below the secant to the zero-slip point (past the tyre's peak the
            // tangent goes flat/negative and an explicit step would flip the wheel back and forth every tick)
            const float sZero = vl / w.radius;
            const float secant = std::fabs(s0 - sZero) > 1e-3f ? fx0 / (s0 - sZero) : 0.0f;
            const float kx = std::max({0.0f, (fxOf(s0 + eps, nullptr) - fx0) / eps, secant});
            // brake torque opposes the wheel's rotation inside the same implicit step (it can stop the wheel, never reverse it)
            const float tb = s0 > 0 ? -brakeT : (s0 < 0 ? brakeT : 0.0f);
            float s1 = s0 + (driveT + tb - fx0 * w.radius) * dt / (wheelInertia + kx * w.radius * dt);
            if (brakeT > 0 && std::fabs(driveT) < brakeT && ((s0 > 0 && s1 < 0) || (s0 < 0 && s1 > 0))) s1 = 0;
            w.spinRate = s1;
            // arcade traction control: driven wheels never exceed ~12% slip (excess engine torque is lost)
            if (w.driven) {
                static const float kTcs = std::getenv("TCS") ? float(std::atof(std::getenv("TCS"))) : 0.06f;
                static const float kTcsMin = std::getenv("TCSMIN") ? float(std::atof(std::getenv("TCSMIN"))) : 0.12f;
                const float lim = std::max(kTcs * denom, kTcsMin);
                if (driveT > 0) w.spinRate = std::min(w.spinRate, std::max(s0 * 0.0f + (vl + lim) / w.radius, std::min(s0, (vl + lim) / w.radius)));
                else if (driveT < 0 && gear == 0) w.spinRate = std::max(w.spinRate, (vl - lim) / w.radius);
            }
        }
        float Fx = fxOf(w.spinRate, &peakX);
        peakX *= grip;
        // friction ellipse
        const float ex = peakX > 1 ? Fx / peakX : 0, ey = peakY > 1 ? Fy / peakY : 0;
        const float e = std::sqrt(ex * ex + ey * ey);
        if (e > 1.0f) { Fx /= e; Fy /= e; }
        // low-speed lateral stabilisation (keeps a parked car from creeping)
        if (std::fabs(vl) < 2.0f) Fy = std::clamp(Fy + vs * Fs * 0.5f, -peakY, peakY);
        const V3 force = n * Fs + wf * Fx - ws * Fy;
        w.dbgFx = Fx; w.dbgFy = Fy; w.dbgFz = Fz; w.dbgSR = (w.spinRate * w.radius - vl) / denom * 100.0f; w.dbgSA = slipAngle;
        F += force;
        // Force_Offset (original param): tyre forces act at the centre-of-mass height (+offset.y), not at the contact
        // patch, so accelerating/braking/cornering doesn't shift load between the wheels (arcade behaviour)
        static const bool kFoOff = std::getenv("NOFO") != nullptr;
        const V3 Pf = kFoOff ? P0 : P0 + up * (dot(pos - P0, up) + P.forceOffset.y);
        T += cross(P0 - pos, n * Fs) + cross(Pf - pos, wf * Fx - ws * Fy);
    }
    // aftertouch (Gen_DefaultAfterTouch / Gen_HandbrakeAfterTouch): the driver's steering rotates the car directly
    // about its up axis, on top of the tyre forces — tuned against the original's measured yaw response
    {
        static const float kAt = std::getenv("ATK") ? float(std::atof(std::getenv("ATK"))) : 152.3f;
        int nContact = 0;
        for (auto& w : wheels) nContact += w.contact ? 1 : 0;
        const float steerN = P.maxSteer > 0 ? steerState_ / P.maxSteer : 0.0f;
        static const float kHbAt = std::getenv("HBATK") ? float(std::atof(std::getenv("HBATK"))) : 0.0f;
        const float at = in.handbrake ? P.handbrakeAfterTouch * kHbAt : P.defaultAfterTouch;
        const float speedF = std::clamp(std::fabs(vf) / 10.0f, 0.0f, 1.0f) * (vf < 0 ? -1.0f : 1.0f);
        // fades out as the car's own yaw rate approaches the original's plateau (~105 deg/s for the Mini at 75 km/h)
        static const float kWmax = std::getenv("WMAX") ? float(std::atof(std::getenv("WMAX"))) : 0.5888f;
        const float wy = dot(angVel, up) * (steerN >= 0 ? 1.0f : -1.0f) * (vf < 0 ? -1.0f : 1.0f);
        const float fade = std::clamp(1.0f - wy / kWmax, 0.0f, 1.0f);
        const float yawAcc = steerN * at * kAt * speedF * fade * (nContact / 4.0f);
        T += up * (yawAcc * P.mass * P.inertia.y);
    }
    // arcade stabiliser: pulls roll/pitch back towards upright and damps roll/pitch rate (not yaw)
    {
        static const float kStab = std::getenv("STAB") ? float(std::atof(std::getenv("STAB"))) : 14.0f;
        const V3 worldUp{0, 1, 0};
        const V3 tilt = cross(up, worldUp);               // axis * sin(angle)
        V3 wRP = angVel - up * dot(angVel, up);           // roll + pitch rate
        T += tilt * (kStab * P.mass) - wRP * (2.5f * P.mass);
    }
    // aerodynamic + rolling resistance
    static const float kDragK = std::getenv("DRAGK") ? float(std::atof(std::getenv("DRAGK"))) : 1.349f;
    // original: with the throttle released the car holds its speed (no drag measured while coasting)
    static const float kDragCoast = std::getenv("DRAGC") ? float(std::atof(std::getenv("DRAGC"))) : 0.1229f;
    const float dragMul = kDragCoast + (1.0f - kDragCoast) * std::clamp(throttleState_, 0.0f, 1.0f);
    F -= vel * ((kDragK * speed + P.rollingDrag * 0.5f) * dragMul);

    // ---- integrate rigid body ----
    vel += F * (dt / P.mass);
    static const float kInertiaK = std::getenv("INERTK") ? float(std::atof(std::getenv("INERTK"))) : 3.103f;
    const V3 I{P.mass * P.inertia.x * kInertiaK, P.mass * P.inertia.y * kInertiaK, P.mass * P.inertia.z * kInertiaK};
    V3 tl = rot.unrotate(T);
    V3 wl = rot.unrotate(angVel);
    wl = wl + V3{tl.x / I.x, tl.y / I.y, tl.z / I.z} * dt;
    angVel = rot.rotate(wl);
    pos += vel * dt;
    Quat dq{0, angVel.x, angVel.y, angVel.z};
    Quat qd = dq * rot;
    rot.w += 0.5f * qd.w * dt; rot.x += 0.5f * qd.x * dt; rot.y += 0.5f * qd.y * dt; rot.z += 0.5f * qd.z * dt;
    rot.normalize();

    // ---- chassis vs. world (walls, kerbs, roof) ----
    // low probes sit above kerb height: the original rides over kerbs on its wheels instead of hitting them
    static const float kLow = std::getenv("PROBELOW") ? float(std::atof(std::getenv("PROBELOW"))) : 0.35f;
    const V3 probes[] = {{0.75f, kLow, 1.55f}, {-0.75f, kLow, 1.55f}, {0.75f, kLow, -1.45f}, {-0.75f, kLow, -1.45f},
                         {0.75f, 0.9f, 0.6f}, {-0.75f, 0.9f, 0.6f}, {0.75f, 0.9f, -0.9f}, {-0.75f, 0.9f, -0.9f},
                         {0.0f, 1.3f, 0.0f}, {0.0f, kLow + 0.05f, 1.65f}, {0.0f, kLow + 0.05f, -1.55f}};
    // Rigid-body contact impulses: low restitution and sliding friction, so the car thuds and scrapes
    // along walls instead of bouncing off them.
    const V3 org = pos - rot.rotate(P.centerOfMass);
    const float invM = 1.0f / P.mass;
    auto invI = [&](const V3& wv) { V3 l = rot.unrotate(wv); return rot.rotate({l.x / I.x, l.y / I.y, l.z / I.z}); };
    // walls only get a fraction of the angular response (arcade: the car glances off instead of spinning like a top)
    float angScale = 1.0f;
    auto applyImpulse = [&](const V3& r, const V3& J) { vel += J * invM; angVel += invI(cross(r, J)) * angScale; };
    auto effMass = [&](const V3& r, const V3& n) { return invM + angScale * dot(cross(invI(cross(r, n)), r), n); };
    bool touching = false;
    float slide = 0;
    dbgProbes.clear();
    for (const V3& pl : probes) {
        const V3 p = org + rot.rotate(pl);
        const V3 d = p - pos;
        const float len = length(d);
        RayHit hit;
        if (!world.raycast(pos, d, len, hit)) continue;
        const V3 n = hit.normal;
        // penetration measured along the surface normal (not along the probe ray)
        const float depth = std::min(dot(hit.point - p, n), 0.25f);
        if (depth <= 0) continue;
        {
            char b[96];
            std::snprintf(b, sizeof b, " [%d n=(%.2f %.2f %.2f) d=%.3f]", int(&pl - probes), n.x, n.y, n.z, depth);
            dbgProbes += b;
        }
        pos += n * depth;
        const V3 r = hit.point - pos;
        const V3 vp = vel + cross(angVel, r);
        const float vn = dot(vp, n);
        if (vn >= 0) continue;
        touching = true;
        const bool wall = std::fabs(n.y) < 0.7f;
        static const float kWallAng = std::getenv("WALLANG") ? float(std::atof(std::getenv("WALLANG"))) : 0.375f;
        static const float kWallE = std::getenv("WALLE") ? float(std::atof(std::getenv("WALLE"))) : 0.15f;
        static const float kWallMu = std::getenv("WALLMU") ? float(std::atof(std::getenv("WALLMU"))) : 0.0395f;
        angScale = wall ? kWallAng : 1.0f;
        const float restitution = wall ? kWallE : 0.12f, mu = wall ? kWallMu : 0.3f;
        const float j = -(1.0f + restitution) * vn / effMass(r, n);
        applyImpulse(r, n * j);
        if (-vn > 1.5f && std::fabs(n.y) < 0.7f) impact = std::max(impact, -vn);
        const V3 vp2 = vel + cross(angVel, r);
        const V3 vt = vp2 - n * dot(vp2, n);
        const float tl2 = length(vt);
        if (tl2 > 1e-3f) {
            const V3 tdir = vt * (1.0f / tl2);
            const float jt = std::min(tl2 / effMass(r, tdir), mu * j);
            applyImpulse(r, tdir * -jt);
            if (std::fabs(n.y) < 0.7f) slide = std::max(slide, tl2);
        }
    }
    scrape = touching ? std::max(scrape * 0.9f, slide) : scrape * 0.97f;
    // arcade recovery: a car resting on its side or roof is put back on its wheels
    const V3 upNow = rot.rotate({0, 1, 0});
    if (upNow.y < 0.35f && length(vel) < 3.0f) flipTimer_ += dt; else flipTimer_ = 0;
    if (flipTimer_ > 1.2f) {
        const V3 f = rot.rotate({0, 0, 1});
        const float heading = std::atan2(f.x, f.z);
        const V3 at = pos - V3{0, P.centerOfMass.y, 0} + V3{0, 0.6f, 0};
        const int g = gear;
        reset(at, heading);
        gear = g > 0 ? 1 : 0;
        flipTimer_ = 0;
    }
    if (pos.y < -60.f) reset({25.5f, 2.7f, 77.1f}, kPi * 0.5f);
}

} // namespace gti
