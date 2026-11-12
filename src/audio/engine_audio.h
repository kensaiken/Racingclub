#pragma once
// Engine sound: rpm-driven crossfade of looped samples, configured by <car>_full_audio.xml
// (Sumo "isem_audionodehierarchy": xfadenode -> sample{volume_marker, volume_level, basex} -> audionode).
#include "audio/vag.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

struct SDL_AudioStream;

namespace gti::audio {

class EngineAudio {
public:
    ~EngineAudio();
    bool open();                       // opens the default playback device
    void close();                      // stops the device (call before SDL_Quit)
    bool loadCar(const std::string& fullAudioXml, const std::string& dataDir);
    void setState(float rpm, float throttle, float volume) { rpm_ = rpm; throttle_ = throttle; volume_ = volume; }
    void mix(float* out, int frames);  // called from the audio thread (stereo interleaved)
    // collision / tyre effects (cars/audiodata/impact, scrape, common/skidloop)
    bool loadEffects(const std::string& dataDir);
    void playImpact(float speed);      // speed into the wall, m/s
    void setEffects(float scrapeSpeed, float skid) { scrape_ = scrapeSpeed; skid_ = skid; }

private:
    struct Voice {
        PcmClip clip;
        float marker[4]{}, level[4]{};
        float baseX = 1000.f;
        bool pitched = true;
        double phase = 0;
        float gain = 0;                // smoothed
    };
    struct Layer {
        std::string name;              // e.g. iSXfadeEngineOn
        bool on = true;                // on-throttle layer
        float xMul = 1.0f;
        std::vector<Voice> voices;
    };
    std::vector<Layer> layers_;
    struct Shot { const PcmClip* clip; double phase = 0; float gain = 1; double pitch = 1; };
    struct Loop { PcmClip clip; double phase = 0; float gain = 0; };
    std::vector<PcmClip> impacts_;
    std::vector<Shot> shots_;
    Loop scrapeLoop_, skidLoop_;
    std::atomic<float> scrape_{0}, skid_{0};
    unsigned rng_ = 12345;
    std::mutex mtx_;
    std::atomic<float> rpm_{900}, throttle_{0}, volume_{0.6f};
    float thrSmooth_ = 0;
    SDL_AudioStream* stream_ = nullptr;
    static constexpr int kRate = 48000;
};

} // namespace gti::audio
