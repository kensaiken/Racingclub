// Runs recompiled original code on test inputs.
// usage: recomp_test <EBOOT.elf> tyre      -> original lateral Pacejka (0x31dce8) vs. the port's formula
#include "recomp/guest.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace gti::recomp;

namespace gti::recomp { void f_0031dce8(PPU& c); void f_003367c0(PPU& c); }
#include <vector>
#include <algorithm>

static float port_lat(const float a[15], float slipDeg, float FzN, float camber) {
    const float Fz = FzN * 0.001f;
    const float C = a[0], D = Fz * (a[1] * Fz + a[2]);
    const float BCD = a[3] * std::sin(2.0f * std::atan(Fz / a[4])) * (1.0f - a[5] * std::fabs(camber));
    const float B = BCD / (C * D), E = a[6] * Fz + a[7];
    const float Sh = a[8] * camber + a[9] * Fz + a[10];
    const float Sv = ((a[11] * Fz + a[12]) * camber + a[13]) * Fz + a[14];
    const float x = slipDeg + Sh, bx = B * x;
    return D * std::sin(C * std::atan(bx - E * (bx - std::atan(bx)))) + Sv;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: recomp_test EBOOT.elf tyre\n"); return 1; }
    Guest g;
    if (!g.init(argv[1])) { std::printf("cannot load %s\n", argv[1]); return 1; }
    const std::string what = argv[2];
    if (what == "tyre") {
        // Mini Pacejka_Lat
        const float a[15] = {1.270001f, -22.1f, 1000.0f, 720.0f, 17.620001f, 0.022f, -0.354f, 0.707f, -0.028f, 0, 0, -14.8f, 0, 0, 0};
        const uint32_t obj = 0x40000000;
        int bad = 0;
        double worst = 0;
        for (float Fz : {800.0f, 2500.0f, 4200.0f})
            for (float slip : {-12.0f, -3.0f, -0.5f, 0.0f, 0.7f, 2.0f, 6.0f, 20.0f}) {
                PPU c;
                g.prepare(c);
                for (int i = 0; i < 15; ++i) c.wrf32(obj + 4 + i * 4, a[i]);
                c.wrf32(obj + 0x78, slip);
                c.wrf32(obj + 0x7c, 0.0f);
                c.wrf32(obj + 0x88, Fz);
                c.wrf32(obj + 0x84, -12345.0f);
                c.r[3] = obj;
                f_0031dce8(c);
                const float orig = c.rdf32(obj + 0x84), port = port_lat(a, slip, Fz, 0.0f);
                const double diff = std::fabs(double(orig) - port);
                worst = std::max(worst, diff);
                if (c.missing || c.unimplAt || diff > 1.0) ++bad;
                std::printf("Fz %6.0f slip %6.2f  original %10.3f  port %10.3f  %s%s\n", Fz, slip, orig, port,
                            c.missing ? "MISSING " : "", c.unimplAt ? "UNIMPL" : "");
                if (c.missing) std::printf("   missing call 0x%x\n", c.missing);
            }
        std::printf("worst difference %.4f N, failures %d\n", worst, bad);
        return bad ? 2 : 0;
    }
    if (what == "step" && argc >= 6) {
        // recomp_test EBOOT.elf step snapshot car_hex next.bin  -> one original car update on the snapshot
        if (!g.loadSnapshot(argv[3])) { std::printf("snapshot load failed\n"); return 1; }
        const uint32_t car = uint32_t(std::strtoul(argv[4], nullptr, 16));
        FILE* nf = std::fopen(argv[5], "rb");
        std::vector<uint8_t> nx;
        if (nf) { uint8_t buf[65536]; size_t k; while ((k = std::fread(buf, 1, sizeof buf, nf)) > 0) nx.insert(nx.end(), buf, buf + k); std::fclose(nf); }
        const size_t N = 0x1000, hdr = 16, stride = 8 + N;
        auto frame = [&](size_t i, size_t off) { const uint8_t* b = nx.data() + hdr + i * stride + 8 + off; uint32_t u = uint32_t(b[0]) << 24 | b[1] << 16 | b[2] << 8 | b[3]; float f; std::memcpy(&f, &u, 4); return f; };
        const size_t frames = nx.size() > hdr ? (nx.size() - hdr) / stride : 0;
        PPU c;
        g.prepare(c);
        auto show = [&](const char* tag) {
            std::printf("%-10s pos %9.4f %9.4f %9.4f  vel %8.4f %8.4f %8.4f  angvel %7.4f %7.4f %7.4f\n", tag,
                        c.rdf32(car + 0x90), c.rdf32(car + 0x94), c.rdf32(car + 0x98), c.rdf32(car + 0x140), c.rdf32(car + 0x144), c.rdf32(car + 0x148),
                        c.rdf32(car + 0x150), c.rdf32(car + 0x154), c.rdf32(car + 0x158));
        };
        show("snapshot");
        const double dt = argc >= 7 ? std::atof(argv[6]) : 1.0 / 60.0;
        c.r[3] = car;
        c.f[1] = dt;
        std::vector<uint8_t> before(c.mem + car, c.mem + car + 0x1000);
        f_003367c0(c);
        show("recomp");
        int changed = 0;
        for (uint32_t o = 0; o < 0x1000; o += 4)
            if (std::memcmp(before.data() + o, c.mem + car + o, 4) != 0 && changed++ < 60) {
                float fb, fa; uint32_t ub = uint32_t(before[o]) << 24 | before[o + 1] << 16 | before[o + 2] << 8 | before[o + 3]; std::memcpy(&fb, &ub, 4);
                fa = c.rdf32(car + o);
                const float fe = frames > 1 ? frame(1, o) : 0;
                std::printf("  +%03x  %12.5f -> %12.5f   (emu next %12.5f)\n", o, fb, fa, fe);
            }
        std::printf("changed words: %d\n", changed);
        if (c.missing) std::printf("MISSING call target 0x%x\n", c.missing);
        if (c.unimplAt) std::printf("UNIMPLEMENTED instruction at 0x%x\n", c.unimplAt);
        for (size_t i = 0; i < std::min<size_t>(frames, 3); ++i)
            std::printf("emu f%zu    pos %9.4f %9.4f %9.4f  vel %8.4f %8.4f %8.4f  angvel %7.4f %7.4f %7.4f\n", i,
                        frame(i, 0x90), frame(i, 0x94), frame(i, 0x98), frame(i, 0x140), frame(i, 0x144), frame(i, 0x148), frame(i, 0x150), frame(i, 0x154), frame(i, 0x158));
        return 0;
    }
    return 1;
}
