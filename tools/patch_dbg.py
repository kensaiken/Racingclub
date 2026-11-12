def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/game/vehicle.h', [("        V3 hubLocal;         // render hub position, car local\n", "        V3 hubLocal;         // render hub position, car local\n        float dbgFx = 0, dbgFy = 0, dbgFz = 0, dbgSR = 0, dbgSA = 0;\n")])
patch('src/game/vehicle.cpp', [("        const V3 force = n * Fs + wf * Fx - ws * Fy;\n", "        const V3 force = n * Fs + wf * Fx - ws * Fy;\n        w.dbgFx = Fx; w.dbgFy = Fy; w.dbgFz = Fz; w.dbgSR = slipRatio; w.dbgSA = slipAngle;\n")])
patch('src/tools/physlab.cpp', [("    std::printf(\"maxY", "    std::printf(\"maxY"),
  ("car.wheels[2].contact, car.wheels[3].contact);\n", "car.wheels[2].contact, car.wheels[3].contact);\n        if (f % 6 == 0 && std::getenv(\"WDBG\")) for (auto& wh : car.wheels) std::printf(\"     Fz=%.2f Fx=%+7.0f Fy=%+7.0f SR=%+6.1f SA=%+6.2f spin=%.1f\n\", wh.dbgFz, wh.dbgFx, wh.dbgFy, wh.dbgSR, wh.dbgSA, wh.spinRate);\n"),
  ("#include <cstdio>\n", "#include <cstdio>\n#include <cstdlib>\n")])
print('ok')
