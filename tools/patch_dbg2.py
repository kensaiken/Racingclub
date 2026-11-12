def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.h', [("    float impact = 0;", "    std::string dbgProbes;  // chassis contacts of the last step (debug)\n    float impact = 0;")])
patch('src/game/vehicle.cpp', [
 ("""    bool touching = false;
    float slide = 0;
    for (const V3& pl : probes) {""", """    bool touching = false;
    float slide = 0;
    dbgProbes.clear();
    for (const V3& pl : probes) {"""),
 ("""        if (depth <= 0) continue;
        pos += n * depth;""", """        if (depth <= 0) continue;
        {
            char b[96];
            std::snprintf(b, sizeof b, " [%d n=(%.2f %.2f %.2f) d=%.3f]", int(&pl - probes), n.x, n.y, n.z, depth);
            dbgProbes += b;
        }
        pos += n * depth;"""),
 ("#include <cstdlib>\n", "#include <cstdio>\n#include <cstdlib>\n"),
])
patch('src/main.cpp', [("car.wheels[3].contact, car.wheels[0].compression);", "car.wheels[3].contact, car.wheels[0].compression);\n                if (simlog && (simN % 15) == 0) std::fprintf(simlog, \"   probes:%s  wheels comp %.3f %.3f %.3f %.3f\n\", car.dbgProbes.c_str(), car.wheels[0].compression, car.wheels[1].compression, car.wheels[2].compression, car.wheels[3].compression);")])
print('ok')
