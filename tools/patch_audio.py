def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)

patch('CMakeLists.txt', [("  src/game/vehicle.cpp\n)", "  src/game/vehicle.cpp\n  src/audio/vag.cpp\n  src/audio/engine_audio.cpp\n)")])
patch('src/audio/vag.cpp', [('#include "util/bytes.h"', '#include "util/bytes.h"\n#include <algorithm>')])
patch('src/main.cpp', [
    ('#include "game/vehicle.h"\n', '#include "game/vehicle.h"\n#include "audio/engine_audio.h"\n'),
    ('int cli_drive = -1;', 'int cli_drive = -1;\nbool cli_mute = false;'),
    ('        else if (a == "--drive") cli_drive = std::atoi(argv[i + 1]);', '        else if (a == "--drive") cli_drive = std::atoi(argv[i + 1]);\n        else if (a == "--mute") cli_mute = std::atoi(argv[i + 1]) != 0;'),
    ('    std::string driveName;\n', '    std::string driveName;\n    audio::EngineAudio engineAudio;\n    bool audioOk = false;\n'),
    ('        car.reset(spawn, heading);\n', '''        car.reset(spawn, heading);
        if (!cli_mute) {
            if (!audioOk) audioOk = engineAudio.open();
            fs::path audioXml = cars[driveIdx].parent_path() / (cars[driveIdx].stem().string() + "_full_audio.xml");
            if (audioOk && !engineAudio.loadCar(audioXml.string(), dataDir.string())) SDL_Log("no engine audio for %s", audioXml.string().c_str());
        }
'''),
    ('                    if (k == SDLK_ESCAPE) { if (driving) { driving = false; renderer.removeGroup(1); reload(); } else running = false; }',
     '                    if (k == SDLK_ESCAPE) { if (driving) { driving = false; engineAudio.setState(900, 0, 0); renderer.removeGroup(1); reload(); } else running = false; }'),
    ('            car.update(stepDt, in, world);\n', '            car.update(stepDt, in, world);\n            if (audioOk) engineAudio.setState(car.rpm, in.throttle, 0.7f);\n'),
    ('            V3 want = org - flat * 5.6f + V3{0, 2.1f, 0};', '            V3 want = org - flat * 6.0f + V3{0, 1.7f, 0};'),
    ('            const V3 target3 = org + V3{0, 1.0f, 0};', '            const V3 target3 = org + V3{0, 0.95f, 0};'),
])
print('ok')
