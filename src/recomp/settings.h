#pragma once
// User options of the recompiled game: kept in gtiport.ini next to the executable and edited with the in-game
// options menu (F1). Environment variables used by the test scripts still override them where noted.
namespace gti::recomp {
struct Settings {
    int fullscreen = 0;   // borderless full screen
    int scale = 1;        // internal resolution: multiples of the console's 1280x720
    int gameHz = 60;      // emulated vblank rate (the console has 60); GAMEHZ overrides
    int refresh = 1;      // 1 = switch a display that is not a multiple of the game rate (144 Hz) to 120 Hz while playing
    int language = 3;     // PS3 system language id (1 English, 2 French, 3 Spanish, 4 German, 5 Italian)
    int showFps = 1;      // frame rate in the window title
};
Settings& settings();     // loaded from gtiport.ini on first use
void settings_save();
} // namespace gti::recomp
