def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/tools/physlab.cpp', [
 ("""    if (sc == "wall") {""", """    if (sc == "kerb") {
        const float hgt = std::getenv("KERB") ? float(std::atof(std::getenv("KERB"))) : 0.15f;
        w.addTriangle({4, hgt, -50}, {60, hgt, -50}, {60, hgt, 600});
        w.addTriangle({4, hgt, -50}, {60, hgt, 600}, {4, hgt, 600});
        w.addTriangle({4, 0, -50}, {4, 0, 600}, {4, hgt, 600});
        w.addTriangle({4, 0, -50}, {4, hgt, 600}, {4, hgt, -50});
    }
    if (sc == "wall") {"""),
 ("""        } else if (sc == "wallsteer") {""", """        } else if (sc == "kerb") {
            if (t > 3.0f) in.throttle = 0.6f;
            if (t > 3.0f && t < 3.6f) in.steer = 0.6f;
            if (t > 3.6f && t < 4.2f) in.steer = -0.6f;
        } else if (sc == "wallsteer") {"""),
 ("up.y=%.2f contacts", "up.y=%.2f roll=%+5.1f contacts"),
 ("car.vel.x, car.vel.y, car.vel.z, upv.y, car.wheels", "car.vel.x, car.vel.y, car.vel.z, upv.y, std::asin(std::clamp(dot(upv, car.rot.rotate({1, 0, 0})), -1.0f, 1.0f)) * 57.3f, car.wheels"),
])
print('ok')
