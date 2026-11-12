def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.cpp', [
 ("""    for (auto& w : wheels) if (w.driven) { drivenSpin += w.spinRate; ++nd; }""",
  """    // engine speed follows the driven wheels that touch the ground (a wheel spinning in the air doesn't drag the revs up)
    int ndGround = 0; float groundSpin = 0;
    for (auto& w : wheels) if (w.driven) { drivenSpin += w.spinRate; ++nd; if (w.contact) { groundSpin += w.spinRate; ++ndGround; } }
    if (ndGround) { drivenSpin = groundSpin; nd = ndGround; }"""),
 ("""            const int nSame = 2; // two wheels per axle
            driveT = wheelTorqueTotal * share / float(nSame);""",
  """            // limited-slip: the axle's torque goes to its wheels that have ground contact (previous step)
            const Wheel& other = wheels[i ^ 1];
            const int nSame = (w.contact && other.contact) || (!w.contact && !other.contact) ? 2 : 1;
            driveT = (!w.contact && other.contact) ? 0.0f : wheelTorqueTotal * share / float(nSame);"""),
])
print('ok')
