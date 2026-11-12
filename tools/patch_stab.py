def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""        w.compression = std::clamp(P.springLength + w.radius - hit.t, 0.0f, P.springLength + 0.1f);""",
  """        w.compression = std::clamp(P.springLength + w.radius - hit.t, 0.0f, P.springLength + 0.1f);
        // the wheel rides up a kerb over a few steps instead of teleporting onto it
        w.compression = std::min(w.compression, w.prevCompression + 1.5f * dt);"""),
 ("""    // aerodynamic + rolling resistance""",
  """    // arcade stabiliser: pulls roll/pitch back towards upright and damps roll/pitch rate (not yaw)
    {
        static const float kStab = std::getenv("STAB") ? float(std::atof(std::getenv("STAB"))) : 14.0f;
        const V3 worldUp{0, 1, 0};
        const V3 tilt = cross(up, worldUp);               // axis * sin(angle)
        V3 wRP = angVel - up * dot(angVel, up);           // roll + pitch rate
        T += tilt * (kStab * P.mass) - wRP * (2.5f * P.mass);
    }
    // aerodynamic + rolling resistance"""),
])
print('ok')
