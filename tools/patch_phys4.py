def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""    auto applyImpulse = [&](const V3& r, const V3& J) { vel += J * invM; angVel += invI(cross(r, J)); };
    auto effMass = [&](const V3& r, const V3& n) { return invM + dot(cross(invI(cross(r, n)), r), n); };""",
  """    // walls only get a fraction of the angular response (arcade: the car glances off instead of spinning like a top)
    float angScale = 1.0f;
    auto applyImpulse = [&](const V3& r, const V3& J) { vel += J * invM; angVel += invI(cross(r, J)) * angScale; };
    auto effMass = [&](const V3& r, const V3& n) { return invM + angScale * dot(cross(invI(cross(r, n)), r), n); };"""),
 ("""        touching = true;
        const float restitution = 0.12f, mu = 0.3f;""",
  """        touching = true;
        const bool wall = std::fabs(n.y) < 0.7f;
        angScale = wall ? 0.3f : 1.0f;
        const float restitution = 0.12f, mu = wall ? 0.15f : 0.3f;"""),
])
patch('src/tools/physlab.cpp', [
 ("""    if (sc == "glance") {""", """    if (sc == "glance") { // 15 degree wall coming in from the car's left
        V3 p0{2, 0, 50}, p1{2 - 150 * 0.268f, 0, 200};
        w.addTriangle(p0, p1, p1 + V3{0, 3, 0});
        w.addTriangle(p0, p1 + V3{0, 3, 0}, p0 + V3{0, 3, 0});
    }
    if (sc == "head") {"""),
 ("""        } else if (sc == "wall" || sc == "glance") {""", """        } else if (sc == "wall" || sc == "glance" || sc == "head") {"""),
])
print('ok')
