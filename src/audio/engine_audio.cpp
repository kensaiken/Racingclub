#include "audio/engine_audio.h"
#include "util/bytes.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace gti::audio {

namespace {

std::string attr(const std::string& tagText, const char* name) {
    std::string key = std::string(name) + "=\"";
    size_t p = tagText.find(key);
    if (p == std::string::npos) return {};
    p += key.size();
    size_t e = tagText.find('"', p);
    return tagText.substr(p, e - p);
}
// returns the text of the first <tag ...> (just the opening tag) inside [b, e)
std::string open_tag(const std::string& s, const char* tag, size_t b, size_t e) {
    size_t p = s.find(std::string("<") + tag, b);
    if (p == std::string::npos || p >= e) return {};
    size_t q = s.find('>', p);
    return s.substr(p, q - p);
}
float fval(const std::string& v, float def) {
    if (v.empty()) return def;
    return std::strtof(v.c_str(), nullptr);
}
std::string lower(std::string s) {
    for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void SDLCALL audio_cb(void* user, SDL_AudioStream* stream, int additional, int) {
    auto* self = static_cast<EngineAudio*>(user);
    const int frames = additional / int(sizeof(float) * 2);
    if (frames <= 0) return;
    static thread_local std::vector<float> buf;
    buf.assign(size_t(frames) * 2, 0.f);
    self->mix(buf.data(), frames);
    SDL_PutAudioStreamData(stream, buf.data(), frames * int(sizeof(float) * 2));
}

} // namespace

EngineAudio::~EngineAudio() { close(); }

void EngineAudio::close() {
    if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
}

bool EngineAudio::open() {
    if (stream_) return true;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, this);
    if (!stream_) return false;
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

bool EngineAudio::loadCar(const std::string& xmlPath, const std::string& dataDir) {
    std::vector<uint8_t> raw;
    if (!read_file(xmlPath, raw)) return false;
    const std::string s(raw.begin(), raw.end());
    std::vector<Layer> layers;
    size_t p = 0;
    while ((p = s.find("<xfadenode", p)) != std::string::npos) {
        const size_t end = s.find("</xfadenode>", p);
        if (end == std::string::npos) break;
        Layer L;
        L.name = attr(open_tag(s, "xfadenode", p, end), "name");
        L.on = L.name.find("On") != std::string::npos && L.name.rfind("On") == L.name.size() - 2;
        // xparams: ParentFParam (rpm) * Const
        size_t xp = p;
        while (true) {
            std::string t = open_tag(s, "xparams", xp, end);
            if (t.empty()) break;
            if (attr(t, "var_type") == "Const") L.xMul = fval(attr(t, "value"), 1.0f);
            xp = s.find("<xparams", xp) + 8;
        }
        size_t sp = p;
        while ((sp = s.find("<sample", sp)) != std::string::npos && sp < end) {
            const size_t se = s.find("</sample>", sp);
            Voice v;
            std::string vm = open_tag(s, "volume_marker", sp, se), vl = open_tag(s, "volume_level", sp, se);
            std::string misc = open_tag(s, "misc", sp, se), node = open_tag(s, "audionode", sp, se), file = open_tag(s, "file", sp, se);
            const char* k[4] = {"a", "b", "c", "d"};
            for (int i = 0; i < 4; ++i) { v.marker[i] = fval(attr(vm, k[i]), 0); v.level[i] = fval(attr(vl, k[i]), 0); }
            v.baseX = fval(attr(misc, "basex"), 1000.f);
            v.pitched = attr(misc, "pitchsounds") != "0";
            std::string dir = attr(file, "path");
            for (auto& c : dir) if (c == '\\') c = '/';
            if (lower(dir).rfind("data/", 0) == 0) dir = dir.substr(5);
            const std::string path = (std::filesystem::u8path(dataDir) / lower(dir) / (lower(attr(node, "name")) + ".vag")).string();
            if (load_vag(path, v.clip)) L.voices.push_back(std::move(v));
            else SDL_Log("engine audio: missing %s", path.c_str());
            sp = se;
        }
        if (!L.voices.empty()) layers.push_back(std::move(L));
        p = end;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    layers_ = std::move(layers);
    return !layers_.empty();
}

bool EngineAudio::loadEffects(const std::string& dataDir) {
    namespace fs = std::filesystem;
    const fs::path base = fs::u8path(dataDir) / "cars" / "audiodata";
    std::vector<PcmClip> imp;
    for (const char* name : {"impact_wall_01", "impact_wall_02", "impact_wall_03", "impact_wall_04", "impact_wall_05",
                             "impact_metal_barrier_01", "impact_metal_barrier_02", "impact_metal_barrier_03"}) {
        PcmClip c;
        if (load_vag((base / "impact" / (std::string(name) + ".vag")).string(), c)) imp.push_back(std::move(c));
    }
    Loop sc, sk;
    load_vag((base / "scrape" / "scrape_wall_01_lp.vag").string(), sc.clip);
    load_vag((base / "common" / "skidloop01.vag").string(), sk.clip);
    std::lock_guard<std::mutex> lk(mtx_);
    shots_.clear();
    impacts_ = std::move(imp);
    scrapeLoop_ = std::move(sc);
    skidLoop_ = std::move(sk);
    return !impacts_.empty();
}

void EngineAudio::playImpact(float speed) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (impacts_.empty() || shots_.size() > 6) return;
    rng_ = rng_ * 1664525u + 1013904223u;
    // harder hits pick from the heavier samples (higher indices are the metal barrier crunches)
    const size_t n = impacts_.size();
    size_t i = speed > 9.0f ? (5 + (rng_ >> 8) % 3) % n : ((rng_ >> 8) % 5) % n;
    Shot s;
    s.clip = &impacts_[i];
    s.gain = std::clamp(0.25f + speed / 14.0f, 0.25f, 1.3f);
    s.pitch = 0.92 + double((rng_ >> 16) % 160) / 1000.0;
    shots_.push_back(s);
}

void EngineAudio::mix(float* out, int frames) {
    std::lock_guard<std::mutex> lk(mtx_);
    // one-shot impacts
    for (auto& s : shots_) {
        const auto& smp = s.clip->samples;
        const double step = double(s.clip->sampleRate) / kRate * s.pitch;
        for (int f = 0; f < frames && s.phase + 1 < double(smp.size()); ++f) {
            size_t i0 = size_t(s.phase);
            float a = smp[i0], b = smp[i0 + 1];
            float v = (a + (b - a) * float(s.phase - double(i0))) * s.gain * volume_.load();
            out[f * 2] += v; out[f * 2 + 1] += v;
            s.phase += step;
        }
    }
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.phase + 1 >= double(s.clip->samples.size()); }), shots_.end());
    // looped scrape / skid
    auto loop = [&](Loop& L, float target, double pitch) {
        const size_t n = L.clip.samples.size();
        if (!n || (target < 1e-4f && L.gain < 1e-4f)) { L.gain = 0; return; }
        const double step = double(L.clip.sampleRate) / kRate * pitch;
        const float g0 = L.gain, dg = (target - g0) / float(frames);
        for (int f = 0; f < frames; ++f) {
            size_t i0 = size_t(L.phase);
            float a = L.clip.samples[i0 % n], b = L.clip.samples[(i0 + 1) % n];
            float v = (a + (b - a) * float(L.phase - double(i0))) * (g0 + dg * f);
            out[f * 2] += v; out[f * 2 + 1] += v;
            L.phase += step;
            if (L.phase >= double(n)) L.phase -= double(n);
        }
        L.gain = target;
    };
    const float sc = scrape_.load(), sk = skid_.load(), vol0 = volume_.load();
    loop(scrapeLoop_, std::clamp((sc - 0.5f) / 8.0f, 0.0f, 1.0f) * 0.7f * vol0, 0.85 + 0.3 * std::clamp(sc / 15.0f, 0.0f, 1.0f));
    loop(skidLoop_, std::clamp(sk, 0.0f, 1.0f) * 0.4f * vol0, 0.95 + 0.1 * sk);
    const float rpm = rpm_.load(), thr = throttle_.load(), vol = volume_.load();
    // smooth on/off throttle blend per block
    thrSmooth_ += (thr - thrSmooth_) * std::min(1.0f, frames / 2400.0f);
    for (auto& L : layers_) {
        const float x = rpm * L.xMul;
        const float layerGain = (L.on ? thrSmooth_ : 1.0f - thrSmooth_) * 0.5f;
        for (auto& v : L.voices) {
            // piecewise-linear volume envelope over the four rpm markers
            float g;
            if (x <= v.marker[0]) g = v.level[0];
            else if (x >= v.marker[3]) g = v.level[3];
            else {
                int i = x < v.marker[1] ? 0 : (x < v.marker[2] ? 1 : 2);
                float span = std::max(1.0f, v.marker[i + 1] - v.marker[i]);
                g = v.level[i] + (v.level[i + 1] - v.level[i]) * (x - v.marker[i]) / span;
            }
            const float target = g * layerGain * vol;
            if (target < 1e-4f && v.gain < 1e-4f) { v.gain = 0; continue; }
            const double pitch = v.pitched ? double(x) / double(v.baseX) : 1.0;
            const double step = double(v.clip.sampleRate) / kRate * std::clamp(pitch, 0.25, 4.0);
            const size_t n = v.clip.samples.size();
            const float g0 = v.gain, dg = (target - g0) / float(frames);
            for (int f = 0; f < frames; ++f) {
                size_t i0 = size_t(v.phase);
                double fr = v.phase - double(i0);
                float a = v.clip.samples[i0 % n], b = v.clip.samples[(i0 + 1) % n];
                float smp = float(a + (b - a) * fr) * (g0 + dg * f);
                out[f * 2] += smp;
                out[f * 2 + 1] += smp;
                v.phase += step;
                if (v.phase >= double(n)) v.phase -= double(n);
            }
            v.gain = target;
        }
    }
    for (int i = 0; i < frames * 2; ++i) out[i] = std::tanh(out[i]); // soft clip
}

} // namespace gti::audio
