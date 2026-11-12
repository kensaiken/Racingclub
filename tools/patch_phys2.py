def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("    return magic(alphaDeg + Sh, B, C, D, E) + Sv;", "    (void)Sh; (void)Sv; // shifts are side-specific in the original; ignoring them keeps the car symmetric\n    return magic(alphaDeg, B, C, D, E);"),
 ("    return magic(slipPct + Sh, B, C, D, E) + Sv;", "    (void)Sh; (void)Sv;\n    return magic(slipPct, B, C, D, E);"),
 ("""        if (rpm > maxRpm * 0.95f && gear < ng - 1) { ++gear; shiftTimer_ = P.clutchTimer; }
        else if (rpm < maxRpm * 0.42f && gear > 1) { --gear; shiftTimer_ = P.clutchTimer; }""",
  """        if (rpm > maxRpm * 0.95f && gear < ng - 1) { ++gear; shiftTimer_ = P.clutchTimer; }
        else if (gear > 1 && rpm < maxRpm * 0.45f && rpm * P.gears[gear - 1] / P.gears[gear] < maxRpm * 0.8f) { --gear; shiftTimer_ = P.clutchTimer; }"""),
 ("""        engineTorque = rpm > maxRpm + 200 ? 0 : t * P.torqueMul * throttleState_;""",
  """        engineTorque = rpm > maxRpm + 200 ? 0 : t * P.torqueMul * throttleState_;
        // engine braking when lifting off
        const float maxT = *std::max_element(P.torque.begin(), P.torque.end()) * P.torqueMul;
        if (rpm > 1100) engineTorque -= (1.0f - throttleState_) * 0.25f * maxT * (rpm / maxRpm);"""),
])
print('ok')
