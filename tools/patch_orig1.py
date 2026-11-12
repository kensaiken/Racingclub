def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('src/game/vehicle.cpp', [
    # steering: measured in the original (RPCS3) — normalized steer moves at Gen_SteerSpeed / Gen_RestoreSteerSpeed
    # per second and is NOT reduced with speed
    ("    const float steerLimit = P.maxSteer / (1.0f + speed / 30.0f);", "    const float steerLimit = P.maxSteer;"),
    # drivetrain: measured rpm = 382 per m/s in 1st (Mini) and traction = full torque per driven wheel at an
    # effective radius of 1.279 x the wheel radius
    ("    rpm = std::clamp(std::fabs(drivenSpin * ratio * P.finalDrive) * 60.0f / (2 * kPi), 900.0f, maxRpm + 400.0f);",
     "    static const float kDriveR = std::getenv(\"DRIVER\") ? float(std::atof(std::getenv(\"DRIVER\"))) : 1.279f;\n"
     "    rpm = std::clamp(std::fabs(drivenSpin * ratio * P.finalDrive / kDriveR) * 60.0f / (2 * kPi), 1000.0f, maxRpm + 400.0f);"),
    ("            driveT = (!w.contact && other.contact) ? 0.0f : wheelTorqueTotal * share / float(nSame);",
     "            (void)nSame;\n"
     "            driveT = (!w.contact && other.contact) ? 0.0f : wheelTorqueTotal * share / kDriveR;"),
    # upshift measured at ~6730 rpm with a 6800 rpm curve end
    ("        if (rpm > maxRpm * 0.95f && gear < ng - 1)", "        if (rpm > maxRpm * 0.985f && gear < ng - 1)"),
])
print('ok')
