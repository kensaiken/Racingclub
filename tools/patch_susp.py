def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""        const float compVel = (w.compression - w.prevCompression) / dt;
        float Fs = k * w.compression + c * compVel;
        if (w.compression > P.springLength) Fs += k * 10.0f * (w.compression - P.springLength); // bump stop
""", """        // a wheel climbing a kerb changes compression in a single step: limit the damper speed and the
        // total wheel load so the car rides up instead of being launched
        const float compVel = std::clamp((w.compression - w.prevCompression) / dt, -2.5f, 2.5f);
        float Fs = k * w.compression + c * compVel;
        if (w.compression > P.springLength) Fs += k * 4.0f * (w.compression - P.springLength); // bump stop
        Fs = std::min(Fs, P.mass * 9.81f * 1.6f);
"""),
 ("""    if (pos.y < -60.f) reset({25.5f, 2.7f, 77.1f}, kPi * 0.5f);""",
  """    // arcade recovery: a car resting on its side or roof is put back on its wheels
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
    if (pos.y < -60.f) reset({25.5f, 2.7f, 77.1f}, kPi * 0.5f);"""),
])
patch('src/game/vehicle.h', [("float steerState_ = 0,", "float flipTimer_ = 0, steerState_ = 0,")])
print('ok')
