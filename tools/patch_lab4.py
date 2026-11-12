def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/tools/physlab.cpp', [
 ("""        } else if (sc == "kerb") {""", """        } else if (sc == "brake") {          // original: 63 -> 14 km/h in 1.75 s, then reverse up to ~48 km/h
            static bool braking = false;
            if (car.speedKmh > 63.0f && !braking) braking = true;
            if (braking) { in.throttle = 0; in.brake = 1; }
        } else if (sc == "steer75") {        // original: full lock at 75 km/h -> yaw rate 21/90/108 deg/s after 0.1/0.3/0.5 s
            static float tSteer = -1;
            if (tSteer < 0 && car.speedKmh > 75.0f) tSteer = t;
            if (tSteer >= 0 && t - tSteer < 1.0f) in.steer = 1;
        } else if (sc == "kerb") {"""),
])
print('ok')
