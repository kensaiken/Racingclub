def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('src/main.cpp', [
    ('int cli_drive = -1;', 'int cli_drive = -1;\nint cli_cam = -1;'),
    ('        else if (a == "--mute") cli_mute = std::atoi(argv[i + 1]) != 0;',
     '        else if (a == "--mute") cli_mute = std::atoi(argv[i + 1]) != 0;\n        else if (a == "--cam") cli_cam = std::atoi(argv[i + 1]);'),
    # car extents for the cockpit camera
    ('    std::string driveName;\n', '    std::string driveName;\n    int camMode = 1;            // 0 cockpit, 1 chase near, 2 chase far\n    float carTop = 1.3f, impactCooldown = 0;\n    bool sfxOk = false;\n'),
    ('        renderer.addPackage(carPkg, 1);\n', '''        renderer.addPackage(carPkg, 1);
        {
            float top = 0;
            for (auto& e : carPkg.entities)
                for (auto& m : e.meshes)
                    for (size_t v = 1; v < m.positions.size(); v += 3) top = std::max(top, m.positions[v] * e.world[5] + e.world[13]);
            carTop = top > 0.6f ? top : 1.3f;
        }
'''),
    ('            if (audioOk && !engineAudio.loadCar(audioXml.string(), dataDir.string())) SDL_Log("no engine audio for %s", audioXml.string().c_str());\n',
     '            if (audioOk && !engineAudio.loadCar(audioXml.string(), dataDir.string())) SDL_Log("no engine audio for %s", audioXml.string().c_str());\n'
     '            if (audioOk && !sfxOk) sfxOk = engineAudio.loadEffects(dataDir.string());\n'),
    ('                    else if (driving && k == SDLK_BACKSPACE) startDrive(driveIdx);\n',
     '                    else if (driving && k == SDLK_BACKSPACE) startDrive(driveIdx);\n'
     '                    else if (driving && k == SDLK_V) { camMode = (camMode + 1) % 3; camInit = false; }\n'),
    ('                case SDL_EVENT_GAMEPAD_ADDED:\n',
     '                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:\n'
     '                    if (driving && (ev.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH || ev.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) { camMode = (camMode + 1) % 3; camInit = false; }\n'
     '                    else if (driving && ev.gbutton.button == SDL_GAMEPAD_BUTTON_START) startDrive(driveIdx);\n'
     '                    break;\n'
     '                case SDL_EVENT_GAMEPAD_ADDED:\n'),
    # sounds from the physics
    ('            if (audioOk) engineAudio.setState(car.rpm, in.throttle, 0.7f);\n', '''            if (audioOk) {
                engineAudio.setState(car.rpm, in.throttle, 0.7f);
                impactCooldown -= stepDt;
                if (car.impact > 0 && impactCooldown <= 0) { engineAudio.playImpact(car.impact); impactCooldown = 0.18f; }
                float skid = 0;
                if (car.speedKmh > 12.0f)
                    for (auto& wh : car.wheels)
                        if (wh.contact) skid = std::max({skid, std::clamp((std::fabs(wh.dbgSA) - 7.0f) / 10.0f, 0.0f, 1.0f),
                                                         std::clamp((std::fabs(wh.dbgSR) - 20.0f) / 40.0f, 0.0f, 1.0f)});
                engineAudio.setEffects(car.scrape, skid);
            }
            car.impact = 0;
'''),
    # cameras
    ('''            const V3 target3 = org + V3{0, 0.95f, 0};
            V3 want = org - flat * 6.0f + V3{0, 1.7f, 0};
            if (!camInit) { camEye[0] = want.x; camEye[1] = want.y; camEye[2] = want.z; camInit = true; }
            const float a = 1.0f - std::exp(-8.0f * stepDt);
            camEye[0] += (want.x - camEye[0]) * a; camEye[1] += (want.y - camEye[1]) * a; camEye[2] += (want.z - camEye[2]) * a;
            for (int k = 0; k < 3; ++k) eye[k] = camEye[k];
            look[0] = target3.x + flat.x * 2.f; look[1] = target3.y; look[2] = target3.z + flat.z * 2.f;
            znear = 0.1f; zfar = 5000.f;''',
     '''            if (cli_cam >= 0) { camMode = cli_cam % 3; cli_cam = -1; }
            if (camMode == 0) {
                // cockpit: rigid with the car, driver's seat (left-hand drive: car +X is left)
                const V3 e3 = org + car.rot.rotate(V3{0.32f, carTop - 0.30f, -0.05f});
                const V3 l3 = e3 + cfwd * 10.0f;
                const V3 u3 = car.rot.rotate(V3{0, 1, 0});
                eye[0] = e3.x; eye[1] = e3.y; eye[2] = e3.z;
                look[0] = l3.x; look[1] = l3.y; look[2] = l3.z;
                up[0] = u3.x; up[1] = u3.y; up[2] = u3.z;
                znear = 0.03f;
            } else {
                const bool farCam = camMode == 2;
                const float back = farCam ? 9.0f : 6.0f, height = farCam ? 2.9f : 1.7f;
                const V3 target3 = org + V3{0, farCam ? 1.1f : 0.95f, 0};
                V3 want = org - flat * back + V3{0, height, 0};
                if (!camInit) { camEye[0] = want.x; camEye[1] = want.y; camEye[2] = want.z; camInit = true; }
                const float a = 1.0f - std::exp(-8.0f * stepDt);
                camEye[0] += (want.x - camEye[0]) * a; camEye[1] += (want.y - camEye[1]) * a; camEye[2] += (want.z - camEye[2]) * a;
                for (int k = 0; k < 3; ++k) eye[k] = camEye[k];
                look[0] = target3.x + flat.x * 2.f; look[1] = target3.y; look[2] = target3.z + flat.z * 2.f;
                znear = 0.1f;
            }
            zfar = 5000.f;'''),
    ('N/B coche  Retroceso reiniciar  Esc salir', 'V camara  N/B coche  Retroceso reiniciar  Esc salir'),
])
print('ok')
