p = 'src/main.cpp'
s = open(p, encoding='utf-8').read()

def rep(a, b):
    global s
    assert a in s, a[:80]
    s = s.replace(a, b, 1)

rep('#include "render/renderer.h"\n', '#include "render/renderer.h"\n#include "game/collision.h"\n#include "game/vehicle.h"\n')
rep('bool cli_nolm = false;', 'bool cli_nolm = false;\nint cli_drive = -1;\nfloat cli_sim = 0.f, cli_simSteer = 0.f;')
rep('        else if (a == "--nolm") cli_nolm = true;', '''        else if (a == "--nolm") cli_nolm = true;
        else if (a == "--drive") cli_drive = std::atoi(argv[i + 1]);
        else if (a == "--sim") cli_sim = float(std::atof(argv[i + 1]));
        else if (a == "--simsteer") cli_simSteer = float(std::atof(argv[i + 1]));''')
rep('    if (!SDL_Init(SDL_INIT_VIDEO)) {', '    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {')
rep('''    reload();
    if (!shotFile.empty()) { yaw = shotYaw; pitch = shotPitch; }
''', '''    reload();
    if (!shotFile.empty()) { yaw = shotYaw; pitch = shotPitch; }

    // ------------------------------------------------------------------ driving
    bool driving = false;
    int driveIdx = 0;
    std::unique_ptr<ep3::Package> trackPkg, levelPkg;
    CollisionWorld world;
    Vehicle car;
    float camEye[3]{0, 0, 0};
    bool camInit = false;
    SDL_Gamepad* pad = nullptr;
    std::string driveName;
    auto startDrive = [&](int carIdx) {
        if (cars.empty() || dataDir.empty()) return;
        driveIdx = (carIdx % int(cars.size()) + int(cars.size())) % int(cars.size());
        const fs::path lvlDir = dataDir / "levels" / "cote d'azur";
        if (!trackPkg) {
            trackPkg = std::make_unique<ep3::Package>();
            if (!trackPkg->load((lvlDir / "afterburn.ep3").string())) { trackPkg.reset(); return; }
            levelPkg = std::make_unique<ep3::Package>();
            levelPkg->load((lvlDir / "cote d'azur.ep3").string());
            world = CollisionWorld{};
            world.addPackage(*trackPkg);
            world.build(4.0f);
        }
        if (!driving) {
            renderer.setPackage(*trackPkg);
            renderer.env = render::Environment{};
            setup_level(renderer, lvlDir);
        }
        renderer.removeGroup(1);
        ep3::Package carPkg;
        if (!carPkg.load(cars[driveIdx].string())) return;
        renderer.addPackage(carPkg, 1);
        CarParams cp;
        fs::path phys = cars[driveIdx].parent_path() / (cars[driveIdx].stem().string() + "_phys.xml");
        if (!load_car_params(phys.string(), cp)) SDL_Log("phys xml missing: %s", phys.string().c_str());
        car.setup(cp, carPkg);
        V3 spawn{25.5f, 2.7f, 77.1f};
        float heading = 1.5707963f;
        if (levelPkg) if (auto* l = levelPkg->findLocator("Start_Lower_0")) {
            spawn = {l->matrix[12], l->matrix[13], l->matrix[14]};
            heading = std::atan2(l->matrix[8], l->matrix[10]);
        }
        car.reset(spawn, heading);
        driving = true; fly = false; camInit = false;
        driveName = cars[driveIdx].stem().string();
    };
    if (cli_drive >= 0) startDrive(cli_drive);
''')
rep('''                    if (k == SDLK_ESCAPE) running = false;''', '''                    if (k == SDLK_ESCAPE) { if (driving) { driving = false; renderer.removeGroup(1); reload(); } else running = false; }
                    else if (k == SDLK_G) startDrive(useCars ? index : driveIdx);
                    else if (driving && k == SDLK_N) startDrive(driveIdx + 1);
                    else if (driving && k == SDLK_B) startDrive(driveIdx - 1);
                    else if (driving && k == SDLK_BACKSPACE) startDrive(driveIdx);
                    else if (driving && (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN || k == SDLK_L || k == SDLK_C)) {}''')
rep('''                default: break;
            }
        }''', '''                case SDL_EVENT_GAMEPAD_ADDED:
                    if (!pad) pad = SDL_OpenGamepad(ev.gdevice.which);
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    if (pad && SDL_GetGamepadID(pad) == ev.gdevice.which) { SDL_CloseGamepad(pad); pad = nullptr; }
                    break;
                default: break;
            }
        }''')
rep('''        float eye[3], look[3];
        float up[3] = {0, 1, 0};
        float znear, zfar;
        if (fly) {''', '''        float eye[3], look[3];
        float up[3] = {0, 1, 0};
        float znear, zfar;
        if (driving) {
            CarInput in;
            const bool* ks = SDL_GetKeyboardState(nullptr);
            in.throttle = (ks[SDL_SCANCODE_UP] || ks[SDL_SCANCODE_W]) ? 1.f : 0.f;
            in.brake = (ks[SDL_SCANCODE_DOWN] || ks[SDL_SCANCODE_S]) ? 1.f : 0.f;
            in.steer = float(int(ks[SDL_SCANCODE_LEFT] || ks[SDL_SCANCODE_A]) - int(ks[SDL_SCANCODE_RIGHT] || ks[SDL_SCANCODE_D]));
            in.handbrake = ks[SDL_SCANCODE_SPACE];
            if (pad) {
                float rt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.f, lt = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.f;
                float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
                if (std::fabs(sx) < 0.12f) sx = 0;
                in.throttle = std::max(in.throttle, rt);
                in.brake = std::max(in.brake, lt);
                if (sx != 0) in.steer = -sx;
                in.handbrake = in.handbrake || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
            }
            float stepDt = dt;
            if (cli_sim > 0) { in.throttle = 1.f; in.steer = cli_simSteer; stepDt = 1.f / 60.f; cli_sim -= stepDt; if (cli_sim <= 0) cli_sim = -1; }
            car.update(stepDt, in, world);
            float carM[16];
            const V3 org = car.originWorld();
            to_matrix(car.rot, org, carM);
            renderer.setGroupTransform(1, carM);
            static const char* wn[4] = {"WheelFL", "WheelFR", "WheelRL", "WheelRR"};
            for (int i = 0; i < 4; ++i) {
                const auto& wh = car.wheels[i];
                const float off = wh.contact ? (wh.compression - car.params.springLength * 0.65f) : -car.params.springLength * 0.35f;
                float Tm[16], Rs[16], Rw[16], Tb[16], tmp[16], tmp2[16], loc[16];
                to_matrix(Quat{}, wh.hubLocal + V3{0, off, 0}, Tm);
                to_matrix(Quat::axisAngle({0, 1, 0}, wh.steer), {0, 0, 0}, Rs);
                to_matrix(Quat::axisAngle({1, 0, 0}, wh.spin), {0, 0, 0}, Rw);
                to_matrix(Quat{}, -wh.hubLocal, Tb);
                mat_mul(Tm, Rs, tmp); mat_mul(tmp, Rw, tmp2); mat_mul(tmp2, Tb, loc);
                float full[16]; mat_mul(carM, loc, full);
                renderer.setEntityTransform(1, wn[i], full);
            }
            const V3 cfwd = car.rot.rotate({0, 0, 1});
            V3 flat = normalize(V3{cfwd.x, 0, cfwd.z});
            const V3 target3 = org + V3{0, 1.0f, 0};
            V3 want = org - flat * 5.6f + V3{0, 2.1f, 0};
            if (!camInit) { camEye[0] = want.x; camEye[1] = want.y; camEye[2] = want.z; camInit = true; }
            const float a = 1.0f - std::exp(-8.0f * stepDt);
            camEye[0] += (want.x - camEye[0]) * a; camEye[1] += (want.y - camEye[1]) * a; camEye[2] += (want.z - camEye[2]) * a;
            for (int k = 0; k < 3; ++k) eye[k] = camEye[k];
            look[0] = target3.x + flat.x * 2.f; look[1] = target3.y; look[2] = target3.z + flat.z * 2.f;
            znear = 0.1f; zfar = 5000.f;
            static int titleTick = 0;
            if (++titleTick % 10 == 0) {
                char buf[256];
                std::string g = car.gear == 0 ? std::string("R") : std::to_string(car.gear);
                std::snprintf(buf, sizeof buf, "GTI Club PC  |  %s  |  %3.0f km/h  |  marcha %s  |  %4.0f rpm  |  N/B coche  Retroceso reiniciar  Esc salir",
                              driveName.c_str(), car.speedKmh, g.c_str(), car.rpm);
                SDL_SetWindowTitle(win, buf);
            }
        } else if (fly) {''')
rep('''        if (!shotFile.empty() && ++frame == 3) {''', '''        if (!shotFile.empty() && cli_sim <= 0 && ++frame == 3) {''')
s = s.replace('#include <filesystem>', '#include <filesystem>\n#include <memory>', 1)
open(p, 'w', encoding='utf-8').write(s)
print('patched')
