def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/tools/physlab.cpp', [
 ("""    if (sc == "wall") {""", """    if (sc == "wallsteer" || sc == "wallrev") {
        w.addTriangle({-50, 0, 30}, {50, 0, 30}, {50, 1.0f, 30});
        w.addTriangle({-50, 0, 30}, {50, 1.0f, 30}, {-50, 1.0f, 30});
    }
    if (sc == "wall") {"""),
 ("""        } else if (sc == "wall" ||""", """        } else if (sc == "wallsteer") {
            if (t > 5) in.steer = 1;
        } else if (sc == "wallrev") {
            if (t > 5) { in.throttle = 0; in.brake = 1; in.steer = -1; }
            if (t > 7) { in.throttle = 1; in.brake = 0; in.steer = 1; }
        } else if (sc == "wall" ||"""),
])
print('ok')
