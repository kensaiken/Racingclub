def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""    const float frontShare = std::clamp(params.driveRatio, 0.0f, 1.0f);
    for (int i = 0; i < 4; ++i) wheels[i].driven""",
  """    // the art's hardpoints are a few mm off between left and right; make each axle symmetric so the car runs straight
    for (int a = 0; a < 4; a += 2) {
        Wheel& l = wheels[a]; Wheel& r = wheels[a + 1];
        const float hy = 0.5f * (l.hardpoint.y + r.hardpoint.y), hz = 0.5f * (l.hardpoint.z + r.hardpoint.z);
        const float hx = 0.5f * (std::fabs(l.hardpoint.x) + std::fabs(r.hardpoint.x));
        l.hardpoint = {hx, hy, hz}; r.hardpoint = {-hx, hy, hz};
        const float rad = 0.5f * (l.radius + r.radius);
        l.radius = r.radius = rad;
    }
    const float frontShare = std::clamp(params.driveRatio, 0.0f, 1.0f);
    for (int i = 0; i < 4; ++i) wheels[i].driven"""),
 ("""            w.spinRate = s0 + (driveT - fx0 * w.radius) * dt / (wheelInertia + kx * w.radius * dt);
        }""",
  """            w.spinRate = s0 + (driveT - fx0 * w.radius) * dt / (wheelInertia + kx * w.radius * dt);
            // arcade traction control: driven wheels never exceed ~12% slip (excess engine torque is lost)
            if (w.driven) {
                const float lim = 0.12f * denom;
                if (driveT > 0) w.spinRate = std::min(w.spinRate, std::max(s0 * 0.0f + (vl + lim) / w.radius, std::min(s0, (vl + lim) / w.radius)));
                else if (driveT < 0 && gear == 0) w.spinRate = std::max(w.spinRate, (vl - lim) / w.radius);
            }
        }"""),
 ("""        else if (gear > 1 && rpm < maxRpm * 0.45f && rpm * P.gears[gear - 1] / P.gears[gear] < maxRpm * 0.8f)""",
  """        else if (gear > 1 && rpm < maxRpm * 0.38f && rpm * P.gears[gear - 1] / P.gears[gear] < maxRpm * 0.75f)"""),
])
print('ok')
