def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:60]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('src/main.cpp', [
    # 4 camera modes (cockpit, near, far, cinematic)
    ("                    else if (driving && k == SDLK_V) { camMode = (camMode + 1) % 3; camInit = false; }",
     "                    else if (driving && k == SDLK_V) { camMode = (camMode + 1) % 4; camInit = false; }"),
    ("{ camMode = (camMode + 1) % 3; camInit = false; }", "{ camMode = (camMode + 1) % 4; camInit = false; }"),
    ("            if (cli_cam >= 0) { camMode = cli_cam % 3; cli_cam = -1; }",
     "            if (cli_cam >= 0) { camMode = cli_cam % 4; cli_cam = -1; }"),
    # trackside replay cameras for the cinematic mode
    ("            levelPkg->load((lvlDir / \"cote d'azur.ep3\").string());",
     "            levelPkg->load((lvlDir / \"cote d'azur.ep3\").string());\n"
     "            {\n"
     "                ep3::Package rc;\n"
     "                if (rc.load((lvlDir / \"replaycameras.ep3\").string())) replayCams = rc.cameras;\n"
     "            }"),
    ("    int camMode = 1;", "    std::vector<ep3::Camera> replayCams;\n    int replayCam = -1;\n    float orbitYaw = 0, orbitPitch = 0, fovY = 0.9f;\n    int camMode = 1;"),
    # right stick: look around the car
    ("                float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;",
     "                {\n"
     "                    float rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f, ry = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;\n"
     "                    if (std::fabs(rx) < 0.15f) rx = 0;\n"
     "                    if (std::fabs(ry) < 0.15f) ry = 0;\n"
     "                    const float a = 1.0f - std::exp(-10.0f * dt);\n"
     "                    orbitYaw += (rx * 3.14159f - orbitYaw) * a;     // full stick = look behind\n"
     "                    orbitPitch += (ry * 0.45f - orbitPitch) * a;\n"
     "                }\n"
     "                float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;"),
    # camera placement
    ("""            } else {
                renderer.hiddenGroup = -1;
                const bool farCam = camMode == 2;
                const float back = farCam ? 9.0f : 6.0f, height = farCam ? 2.9f : 1.7f;
                const V3 target3 = org + V3{0, farCam ? 1.1f : 0.95f, 0};
                V3 want = org - flat * back + V3{0, height, 0};""",
     """            } else if (camMode == 3 && !replayCams.empty()) {
                // cinematic: the closest trackside replay camera films the car (switches with hysteresis)
                renderer.hiddenGroup = -1;
                auto dist2 = [&](int i) { const auto& c = replayCams[i]; float dx = c.pos[0] - org.x, dy = c.pos[1] - org.y, dz = c.pos[2] - org.z; return dx * dx + dy * dy + dz * dz; };
                int best = 0;
                for (int i = 1; i < int(replayCams.size()); ++i) if (dist2(i) < dist2(best)) best = i;
                if (replayCam < 0 || dist2(best) < dist2(replayCam) * 0.6f) replayCam = best;
                const auto& rc = replayCams[replayCam];
                for (int k = 0; k < 3; ++k) eye[k] = rc.pos[k];
                look[0] = org.x; look[1] = org.y + 0.8f; look[2] = org.z;
                // zoom so the car keeps roughly the same size on screen (TV-style), within the camera's lens range
                const float d = std::sqrt(dist2(replayCam));
                fovY = std::clamp(2.0f * std::atan(5.0f / std::max(d, 1.0f)), 0.12f, rc.fovDeg * 3.14159f / 180.0f * 1.6f);
                znear = 0.3f;
            } else {
                renderer.hiddenGroup = -1;
                const bool farCam = camMode == 2;
                const float back = farCam ? 9.0f : 6.0f, height = farCam ? 2.9f : 1.7f;
                const V3 target3 = org + V3{0, farCam ? 1.1f : 0.95f, 0};
                // right stick orbits the camera around the car
                const float cy = std::cos(orbitYaw), syw = std::sin(orbitYaw);
                const V3 dir{flat.x * cy - flat.z * syw, 0, flat.x * syw + flat.z * cy};
                V3 want = org - dir * back + V3{0, height + orbitPitch * back, 0};"""),
    ("                look[0] = target3.x + flat.x * 2.f; look[1] = target3.y; look[2] = target3.z + flat.z * 2.f;\n                znear = 0.1f;",
     "                look[0] = target3.x + dir.x * 2.f; look[1] = target3.y; look[2] = target3.z + dir.z * 2.f;\n                znear = 0.1f;\n                fovY = 0.9f;"),
    ("        Mat4 proj = perspective(0.9f, float(w) / float(std::max(h, 1)), znear, zfar);",
     "        if (!driving || camMode == 0) fovY = 0.9f;\n        Mat4 proj = perspective(fovY, float(w) / float(std::max(h, 1)), znear, zfar);"),
    ("if (driving) { driving = false; engineAudio.setState(900, 0, 0); renderer.removeGroup(1); reload(); }",
     "if (driving) { driving = false; renderer.hiddenGroup = -1; engineAudio.setState(900, 0, 0); renderer.removeGroup(1); reload(); }"),
])
print('ok')
