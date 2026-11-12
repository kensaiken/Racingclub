def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""            const float kx = std::max(0.0f, (fxOf(s0 + eps, nullptr) - fx0) / eps);   // dFx / dspin""",
  """            // dFx/dspin: tangent, but never below the secant to the zero-slip point (past the tyre's peak the
            // tangent goes flat/negative and an explicit step would flip the wheel back and forth every tick)
            const float sZero = vl / w.radius;
            const float secant = std::fabs(s0 - sZero) > 1e-3f ? fx0 / (s0 - sZero) : 0.0f;
            const float kx = std::max({0.0f, (fxOf(s0 + eps, nullptr) - fx0) / eps, secant});"""),
])
print('ok')
