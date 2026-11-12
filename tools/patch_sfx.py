def patch(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert a in s, (p, a[:70]); s = s.replace(a, b, 1)
    open(p, 'w', encoding='utf-8').write(s)
patch('src/audio/engine_audio.h', [
 ("""    void mix(float* out, int frames);  // called from the audio thread (stereo interleaved)
""", """    void mix(float* out, int frames);  // called from the audio thread (stereo interleaved)
    // collision / tyre effects (cars/audiodata/impact, scrape, common/skidloop)
    bool loadEffects(const std::string& dataDir);
    void playImpact(float speed);      // speed into the wall, m/s
    void setEffects(float scrapeSpeed, float skid) { scrape_ = scrapeSpeed; skid_ = skid; }
"""),
 ("""    std::vector<Layer> layers_;""", """    std::vector<Layer> layers_;
    struct Shot { const PcmClip* clip; double phase = 0; float gain = 1; double pitch = 1; };
    struct Loop { PcmClip clip; double phase = 0; float gain = 0; };
    std::vector<PcmClip> impacts_;
    std::vector<Shot> shots_;
    Loop scrapeLoop_, skidLoop_;
    std::atomic<float> scrape_{0}, skid_{0};
    unsigned rng_ = 12345;"""),
])
patch('src/audio/engine_audio.cpp', [
 ("""void EngineAudio::mix(float* out, int frames) {
    std::lock_guard<std::mutex> lk(mtx_);""", """bool EngineAudio::loadEffects(const std::string& dataDir) {
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
    loop(skidLoop_, std::clamp(sk, 0.0f, 1.0f) * 0.4f * vol0, 0.95 + 0.1 * sk);"""),
])
print('ok')
