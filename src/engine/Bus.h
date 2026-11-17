#pragma once
#include "Dsp.h"

namespace cr {

enum BusPreset { BUS_CLEAN = 0, BUS_TAPE, BUS_BLOWN, BUS_SMASHED, BUS_BEDROOM, kBusPresets };
enum CrunchType { CR_TAPE = 0, CR_TUBE, CR_CLIP, CR_FUZZ, kCrunchTypes };
inline const char* busPresetName(int p) {
    static const char* n[] = {"Clean", "Tape Warm", "Blown Speaker", "Smashed Room", "Bedroom Crunch"};
    return n[(p >= 0 && p < kBusPresets) ? p : 0];
}

struct BusSettings {
    int preset = BUS_CLEAN;
    float punch = 0, crunch = 0, squash = 0, pump = 0, room = 0;
    int crunchType = CR_TAPE;
    void applyPreset(int p) {
        preset = clampf((float)p, 0, kBusPresets - 1);
        p = preset;
        if (p == BUS_CLEAN) { punch = crunch = squash = pump = room = 0; crunchType = CR_TAPE; }
        else if (p == BUS_TAPE) { punch = 0.25f; crunch = 0.28f; crunchType = CR_TAPE; squash = 0.22f; pump = 0.05f; room = 0.18f; }
        else if (p == BUS_BLOWN) { punch = 0.35f; crunch = 0.62f; crunchType = CR_CLIP; squash = 0.30f; pump = 0.10f; room = 0.08f; }
        else if (p == BUS_SMASHED) { punch = 0.40f; crunch = 0.40f; crunchType = CR_TUBE; squash = 0.72f; pump = 0.45f; room = 0.48f; }
        else if (p == BUS_BEDROOM) { punch = 0.30f; crunch = 0.48f; crunchType = CR_TAPE; squash = 0.35f; pump = 0.22f; room = 0.28f; }
    }
    bool isClean() const { return punch <= 1e-5f && crunch <= 1e-5f && squash <= 1e-5f && pump <= 1e-5f && room <= 1e-5f; }
};

inline float crunchSample(float x, int type, float amt) {
    if (amt <= 1e-5f) return x;
    switch (type) {
        case CR_TUBE: return lerp(x, tubeSat(x, amt), clampf(0.5f + 0.5f * amt, 0, 1));
        case CR_CLIP: return lerp(x, hardClip(x, amt), clampf(0.4f + 0.6f * amt, 0, 1));
        case CR_FUZZ: return lerp(x, fuzzSat(x, amt), clampf(0.35f + 0.65f * amt, 0, 1));
        default: return lerp(x, softTape(x, amt), clampf(0.5f + 0.5f * amt, 0, 1));
    }
}

// Offline render of the drum bus onto a stereo buffer (N samples).
// pumpSrc, if non-null, is a mono kick envelope source of length N (already absolute).
inline void applyBus(float* L, float* R, int N, double sr, const BusSettings& b, const float* pumpSrc = nullptr) {
    if (N <= 0 || b.isClean()) return;
    // Punch: simple transient emphasis — boost the first milliseconds of rising energy.
    if (b.punch > 1e-5f) {
        PeakFollower atk, sus;
        atk.setMs(0.4f, 40.f, sr); sus.setMs(20.f, 180.f, sr);
        for (int i = 0; i < N; ++i) {
            float x = 0.5f * (L[i] + R[i]);
            float a = atk.tick(x), s = sus.tick(x);
            float g = 1.f + b.punch * 1.8f * clampf(a - s, 0, 1);
            L[i] *= g; R[i] *= g;
        }
    }
    if (b.crunch > 1e-5f) {
        for (int i = 0; i < N; ++i) { L[i] = crunchSample(L[i], b.crunchType, b.crunch); R[i] = crunchSample(R[i], b.crunchType, b.crunch); }
        if (b.preset == BUS_BLOWN) {
            // slightly narrower bandwidth
            OnePole lpL, lpR, hpL, hpR; lpL.setHz(6500, sr); lpR.setHz(6500, sr); hpL.setHz(70, sr); hpR.setHz(70, sr);
            for (int i = 0; i < N; ++i) { L[i] = lpL.tick(L[i] - 0.85f * hpL.tick(L[i])); R[i] = lpR.tick(R[i] - 0.85f * hpR.tick(R[i])); }
        }
    }
    if (b.squash > 1e-5f) {
        // Parallel compressor: heavy downward compression, blend under.
        PeakFollower fol; fol.setMs(1.f, 80.f, sr);
        float thresh = 0.12f, ratio = 8.f;
        for (int i = 0; i < N; ++i) {
            float env = fol.tick(0.5f * (L[i] + R[i]));
            float g = 1.f;
            if (env > thresh) g = (thresh + (env - thresh) / ratio) / (env + 1e-8f);
            float makeup = 1.f + 2.2f * b.squash;
            float wetL = L[i] * g * makeup, wetR = R[i] * g * makeup;
            L[i] = lerp(L[i], wetL, b.squash); R[i] = lerp(R[i], wetR, b.squash);
        }
    }
    if (b.pump > 1e-5f && pumpSrc) {
        float rel = 1.f - std::exp(-1.f / (0.18f * (float)sr));
        float env = 0;
        for (int i = 0; i < N; ++i) {
            float t = pumpSrc[i];
            env = t > env ? t : env + rel * (t - env);
            float g = 1.f - b.pump * 0.85f * clampf(env, 0, 1);
            L[i] *= g; R[i] *= g;
        }
    }
    if (b.room > 1e-5f) {
        // Very small Schroeder-ish room: 4 comb + 2 allpass, stereo.
        const int cL[4] = {1553, 1613, 1747, 1889};
        const int cR[4] = {1571, 1663, 1777, 1913};
        std::vector<float> bL[4], bR[4];
        for (int k = 0; k < 4; ++k) { bL[k].assign((size_t)cL[k], 0); bR[k].assign((size_t)cR[k], 0); }
        int iL[4] {}, iR[4] {};
        float fb = 0.72f;
        std::vector<float> wetL((size_t)N, 0), wetR((size_t)N, 0);
        for (int i = 0; i < N; ++i) {
            float sl = 0, sr_ = 0;
            for (int k = 0; k < 4; ++k) {
                float yl = bL[k][(size_t)iL[k]], yr = bR[k][(size_t)iR[k]];
                sl += yl; sr_ += yr;
                bL[k][(size_t)iL[k]] = L[i] + yl * fb; iL[k] = (iL[k] + 1) % cL[k];
                bR[k][(size_t)iR[k]] = R[i] + yr * fb; iR[k] = (iR[k] + 1) % cR[k];
            }
            wetL[(size_t)i] = sl * 0.25f; wetR[(size_t)i] = sr_ * 0.25f;
        }
        for (int i = 0; i < N; ++i) { L[i] += b.room * 0.55f * wetL[(size_t)i]; R[i] += b.room * 0.55f * wetR[(size_t)i]; }
    }
    // gentle limiter
    float ceil = 0.98f;
    for (int i = 0; i < N; ++i) { L[i] = clampf(L[i], -ceil, ceil); R[i] = clampf(R[i], -ceil, ceil); }
}

} // namespace cr
