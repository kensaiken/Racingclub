def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("        T += cross(P0 - pos, force);",
  """        // Force_Offset (original param): tyre forces act at the centre-of-mass height (+offset.y), not at the contact
        // patch, so accelerating/braking/cornering doesn't shift load between the wheels (arcade behaviour)
        static const bool kFoOff = std::getenv("NOFO") != nullptr;
        const V3 Pf = kFoOff ? P0 : P0 + up * (dot(pos - P0, up) + P.forceOffset.y);
        T += cross(P0 - pos, n * Fs) + cross(Pf - pos, wf * Fx - ws * Fy);"""),
 ("    if (auto v = xml_values(x, \"Gen_Inertia\"); v.size() >= 3) p.inertia = {v[0], v[1], v[2]};",
  "    if (auto v = xml_values(x, \"Gen_Inertia\"); v.size() >= 3) p.inertia = {v[0], v[1], v[2]};\n    if (auto v = xml_values(x, \"Force_Offset\"); v.size() >= 3) p.forceOffset = {v[0], v[1], v[2]};"),
])
patch('src/game/vehicle.h', [("    V3 centerOfMass{0, 0.465f, 0.13f}, inertia{1.43f, 1.45f, 0.38f};", "    V3 centerOfMass{0, 0.465f, 0.13f}, inertia{1.43f, 1.45f, 0.38f}, forceOffset{0, 0, 0};")])
print('ok')
