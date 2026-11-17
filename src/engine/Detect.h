#pragma once
#include "Dsp.h"
#include <numeric>

namespace cr {

enum LaneId { LANE_KICK = 0, LANE_SNARE = 1, LANE_HATS = 2, LANE_PERC = 3, kDefaultLanes = 4, kMaxLanes = 8 };

struct Features {
    float thump = 0, snap = 0, sizzle = 0, centroid = 0, duration = 0, zcr = 0, peak = 0;
};

struct Slice {
    int start = 0, end = 0; // samples in the source wave
    int lane = LANE_PERC;
    Features f;
};

inline float bandEnergy(const float* x, int n, double sr, float loHz, float hiHz) {
    // crude Goertzel-ish: one-pole BP energy via difference of lowpasses
    OnePole lo, hi;
    lo.setHz(loHz, sr); hi.setHz(hiHz, sr);
    double e = 0, tot = 0;
    float zlo = 0, zhi = 0;
    float alo = lo.a, ahi = hi.a;
    for (int i = 0; i < n; ++i) {
        zlo += alo * (x[i] - zlo);
        zhi += ahi * (x[i] - zhi);
        float b = zhi - zlo;
        e += (double)b * b;
        tot += (double)x[i] * x[i];
    }
    return tot > 1e-20 ? float(e / tot) : 0;
}

inline Features measureHit(const float* x, int n, double sr) {
    Features f;
    if (n < 8) return f;
    double eTot = 0, eLow = 0, eHigh = 0, eW = 0, zc = 0;
    float prev = 0, pk = 0;
    int hop = std::max(1, (int)(sr * 0.002)); // 2 ms
    // duration: until RMS envelope stays below 12% of peak for 8 ms
    std::vector<float> env((size_t)n, 0);
    float fol = 0, atk = 1.f - std::exp(-1.f / (0.002f * (float)sr)), rel = 1.f - std::exp(-1.f / (0.030f * (float)sr));
    for (int i = 0; i < n; ++i) {
        float a = std::fabs(x[i]);
        fol += (a > fol ? atk : rel) * (a - fol);
        env[(size_t)i] = fol;
        pk = std::max(pk, a);
        eTot += (double)x[i] * x[i];
        if (x[i] >= 0 && prev < 0) ++zc;
        if (x[i] < 0 && prev >= 0) ++zc;
        prev = x[i];
    }
    f.peak = pk;
    // Skip leading silence so a slightly early onset still measures the real hit.
    float attackThr = pk * 0.15f;
    int attack = 0;
    for (int i = 0; i < n; ++i) if (env[(size_t)i] >= attackThr) { attack = i; break; }
    float thresh = pk * 0.12f;
    int hold = (int)(0.008 * sr);
    int below = 0;
    int last = std::max(attack, (int)(0.01 * sr));
    for (int i = attack; i < n; ++i) {
        if (env[(size_t)i] < thresh) { if (++below >= hold) { last = i - hold; break; } }
        else { below = 0; last = i; }
    }
    f.duration = float(std::max(last, 1)) / float(sr);

    // spectral centroid via zero-crossing as a brightness hint plus band energies
    f.zcr = float(zc) * 0.5f / (float(n) / float(sr)); // crossings per second / 2 ≈ freq
    int use = std::min(n, std::max(16, last));
    f.thump = bandEnergy(x, use, sr, 20, 150);
    f.sizzle = bandEnergy(x, use, sr, 5000, 16000);
    // snap: high mid energy in the first 15 ms vs the rest
    int nSnap = std::min(use, (int)(0.015 * sr));
    int nRest = std::max(0, use - nSnap);
    double eSnap = 0, eRest = 0;
    OnePole midLo, midHi; midLo.setHz(800, sr); midHi.setHz(5000, sr);
    float z1 = 0, z2 = 0;
    for (int i = 0; i < use; ++i) {
        z1 += midLo.a * (x[i] - z1); z2 += midHi.a * (x[i] - z2);
        float b = z2 - z1; double e = (double)b * b;
        if (i < nSnap) eSnap += e; else eRest += e;
    }
    double snapDen = eSnap + eRest + 1e-20;
    f.snap = float(eSnap / snapDen);
    if (nSnap > 0) {
        // also reward absolute mid energy in the transient
        f.snap = clampf(f.snap + 0.25f * float(eSnap / (eTot + 1e-20)), 0, 1);
    }
    // centroid: energy-weighted band centres
    float b1 = bandEnergy(x, use, sr, 20, 200);
    float b2 = bandEnergy(x, use, sr, 200, 2000);
    float b3 = bandEnergy(x, use, sr, 2000, 8000);
    float b4 = bandEnergy(x, use, sr, 8000, 16000);
    float wsum = b1 + b2 + b3 + b4 + 1e-8f;
    f.centroid = (b1 * 80 + b2 * 800 + b3 * 4000 + b4 * 11000) / wsum;
    (void)eLow; (void)eHigh; (void)eW; (void)hop; (void)nRest;
    return f;
}

// Rule-based sort. Returns LANE_*.
inline int classifyRules(const Features& f) {
    // Strong, measurable gates first (centroid / sizzle / thump / duration / zcr).
    if (f.thump > 0.4f && f.centroid < 300.f && f.sizzle < 0.08f && f.duration > 0.20f)
        return LANE_KICK;
    if (f.centroid > 4500.f && f.sizzle > 0.16f && f.thump < 0.12f)
        return LANE_HATS;
    if (f.centroid > 1500.f && f.centroid < 4200.f && f.sizzle > 0.08f && f.thump < 0.2f)
        return LANE_SNARE;
    if (f.centroid >= 280.f && f.centroid < 1500.f && f.sizzle < 0.08f && f.duration < 0.28f && f.thump < 0.4f)
        return LANE_PERC;

    float kickScore = 2.0f * f.thump
                    + 1.4f * (f.centroid < 250.f ? 1.f : clampf(1.f - (f.centroid - 250.f) / 350.f, 0, 1))
                    + 0.8f * (f.duration > 0.25f ? 1.f : f.duration / 0.25f)
                    + 0.6f * (f.zcr < 90.f ? 1.f : clampf(1.f - (f.zcr - 90.f) / 200.f, 0, 1))
                    - 1.2f * f.sizzle;
    float snareScore = 1.0f * f.snap
                     + 1.2f * (f.centroid > 1200.f && f.centroid < 4500.f ? 1.f : 0.15f)
                     + 1.0f * clampf(f.sizzle / 0.18f, 0, 1)
                     + 0.5f * (f.zcr > 4000.f && f.zcr < 16000.f ? 1.f : 0.f)
                     - 1.0f * f.thump
                     - 0.8f * (f.centroid > 5000.f ? 1.f : 0.f);
    float hatScore = 2.0f * f.sizzle
                   + 1.4f * (f.centroid > 4000.f ? 1.f : clampf((f.centroid - 2000.f) / 2000.f, 0, 1))
                   + 0.8f * (f.zcr > 8000.f ? 1.f : clampf((f.zcr - 3000.f) / 5000.f, 0, 1))
                   + (f.duration < 0.11f ? 0.8f : (f.duration < 0.20f ? 0.2f : -0.7f))
                   - 1.6f * f.thump;
    float percScore = 0.5f
                    + 1.4f * (f.centroid > 280.f && f.centroid < 1600.f ? 1.f : 0.f)
                    + 0.6f * (f.duration < 0.25f && f.duration > 0.04f ? 1.f : 0.2f)
                    + 0.5f * (f.zcr > 90.f && f.zcr < 800.f ? 1.f : 0.f)
                    - 1.0f * f.sizzle
                    - 0.9f * (f.centroid < 250.f ? 1.f : 0.f)
                    - 0.5f * (f.thump > 0.45f ? f.thump : 0.f);

    float best = kickScore; int lane = LANE_KICK;
    if (snareScore > best) { best = snareScore; lane = LANE_SNARE; }
    if (hatScore > best) { best = hatScore; lane = LANE_HATS; }
    if (percScore > best) { best = percScore; lane = LANE_PERC; }
    return lane;
}

// Tiny linear classifier (4 classes × 6 features + bias). Weights filled by tools/train_classifier
// after a box-CPU training run; zeros mean "use rules only".
struct LinearClf {
    // w[class][thump, snap, sizzle, centroid/8000, duration, zcr/8000, bias]
    float w[4][7] {};
    bool trained = false;
    int classify(const Features& f) const {
        if (!trained) return classifyRules(f);
        float x[7] = { f.thump, f.snap, f.sizzle, f.centroid / 8000.f, f.duration, f.zcr / 8000.f, 1.f };
        int best = 0; float bestS = -1e9f;
        for (int c = 0; c < 4; ++c) {
            float s = 0; for (int i = 0; i < 7; ++i) s += w[c][i] * x[i];
            if (s > bestS) { bestS = s; best = c; }
        }
        return best;
    }
};

inline LinearClf& defaultClf() {
    static LinearClf c;
    return c;
}

inline int classifyHit(const Features& f) { return defaultClf().classify(f); }

// Spectral-flux onset detection. Returns onset sample indices, min 30 ms apart.
inline std::vector<int> detectOnsets(const float* x, int n, double sr, float sensitivity = 1.f) {
    std::vector<int> on;
    if (n < 64) return on;
    int hop = std::max(8, (int)(sr * 0.005)); // 5 ms
    int win = hop * 4;                        // 20 ms
    int frames = 1 + (n - win) / hop;
    if (frames < 4) return on;

    // Per-band *relative* energy rise. Quiet hats after a loud kick still
    // fire because a rise-from-silence in the high band is large relative
    // to that band's recent energy, even if it is small in absolute terms.
    std::vector<float> flux((size_t)frames, 0), energy((size_t)frames, 0);
    float prevLo = 0, prevMid = 0, prevHi = 0;
    float pkAbs = 0;
    for (int i = 0; i < n; ++i) pkAbs = std::max(pkAbs, std::fabs(x[i]));
    float absFloor = std::max(1e-10f, pkAbs * pkAbs * 1e-6f);
    for (int f = 0; f < frames; ++f) {
        int a = f * hop;
        OnePole lp120, lp400, lp2k, lp8k;
        lp120.setHz(120, sr); lp400.setHz(400, sr); lp2k.setHz(2000, sr); lp8k.setHz(8000, sr);
        double eLo = 0, eMid = 0, eHi = 0, eAll = 0;
        for (int i = 0; i < win && a + i < n; ++i) {
            float s = x[a + i];
            float z120 = lp120.tick(s), z400 = lp400.tick(s), z2k = lp2k.tick(s), z8k = lp8k.tick(s);
            float lo = z120, mid = z2k - z400, pre = z8k - z2k, hi = s - z8k;
            eLo += (double)lo * lo; eMid += (double)mid * mid + (double)pre * pre; eHi += (double)hi * hi;
            eAll += (double)s * s;
        }
        float inv = 1.f / float(std::max(1, win));
        float lo = float(eLo) * inv, mid = float(eMid) * inv, hi = float(eHi) * inv;
        auto rel = [&](float e, float prev) {
            float rise = e - prev;
            if (rise <= 0) return 0.f;
            return rise / (prev + absFloor);
        };
        float d = 1.4f * rel(lo, prevLo) + 1.2f * rel(mid, prevMid) + 1.6f * rel(hi, prevHi);
        prevLo = lo; prevMid = mid; prevHi = hi;
        flux[(size_t)f] = d;
        energy[(size_t)f] = float(eAll) * inv;
    }
    auto sm = flux;
    for (int f = 1; f < frames - 1; ++f)
        sm[(size_t)f] = 0.25f * flux[(size_t)f - 1] + 0.5f * flux[(size_t)f] + 0.25f * flux[(size_t)f + 1];
    flux.swap(sm);

    int minGap = std::max(1, (int)std::lround(0.030 * sr / hop));
    float sens = std::max(0.3f, sensitivity);
    // Relative-flux threshold: a local peak several times the local median.
    int last = -minGap * 2;
    int half = std::max(6, (int)std::lround(0.20 * sr / hop));
    const float relThr = 2.0f / sens;
    for (int f = 1; f < frames - 1; ++f) {
        float v = flux[(size_t)f];
        if (v < flux[(size_t)f - 1] || v < flux[(size_t)f + 1]) continue;
        if (f - last < minGap) continue;
        if (energy[(size_t)f] < absFloor * 8.f && v < 1.f) continue; // ignore empty frames
        int a = std::max(0, f - half), b = std::min(frames, f + half);
        std::vector<float> loc(flux.begin() + a, flux.begin() + b);
        std::nth_element(loc.begin(), loc.begin() + loc.size() / 2, loc.end());
        float lmed = loc[loc.size() / 2];
        float thr = std::max(0.8f / sens, lmed * relThr);
        if (v > thr) {
            // Flux windows start up to `win` before the hit. Prefer the
            // sample-accurate attack: peak of a short envelope in the window,
            // then walk back to 15% of that peak.
            int searchA = std::max(0, f * hop);
            int searchB = std::min(n, f * hop + win + hop * 2);
            float e = 0, atkA = 1.f - std::exp(-1.f / (0.0015f * (float)sr));
            float bestE = 0; int bestI = searchA;
            for (int i = searchA; i < searchB; ++i) {
                e += atkA * (std::fabs(x[i]) - e);
                if (e > bestE) { bestE = e; bestI = i; }
            }
            int start = bestI;
            float gate = bestE * 0.15f;
            e = 0;
            // recompute env up to the peak and pick the first crossing
            for (int i = searchA; i <= bestI; ++i) {
                e += atkA * (std::fabs(x[i]) - e);
                if (e >= gate) { start = i; break; }
            }
            start = std::max(0, start - (int)(0.002 * sr));
            if (!on.empty() && start - on.back() < (int)(0.030 * sr)) {
                // too close to the previous slice: overlapping / same hit
                continue;
            }
            on.push_back(start);
            last = f;
        }
    }
    if (!on.empty() && on[0] > (int)(0.04 * sr)) {
        float pk = 0; int span = std::min(n, (int)(0.02 * sr));
        for (int i = 0; i < span; ++i) pk = std::max(pk, std::fabs(x[i]));
        if (pk > 0.05f * std::max(pkAbs, 1e-6f) && pk > 0.02f) on.insert(on.begin(), 0);
    } else if (on.empty()) {
        if (pkAbs > 0.02f) on.push_back(0);
    }
    return on;
}

inline std::vector<Slice> sliceAndClassify(const Wave& w) {
    std::vector<Slice> out;
    int n = w.n();
    if (n < 32) return out;
    std::vector<float> mono((size_t)n);
    for (int i = 0; i < n; ++i) mono[(size_t)i] = w.mono(i);
    auto ons = detectOnsets(mono.data(), n, w.sr);
    for (size_t i = 0; i < ons.size(); ++i) {
        Slice s;
        s.start = ons[i];
        int cap = (i + 1 < ons.size()) ? ons[i + 1] : n;
        int len = std::max(1, cap - s.start);
        auto f = measureHit(mono.data() + s.start, len, w.sr);
        int dur = std::min(len, std::max((int)(0.03 * w.sr), (int)(f.duration * w.sr)));
        s.end = s.start + dur;
        if (s.end <= s.start + 8) s.end = std::min(n, s.start + (int)(0.05 * w.sr));
        // don't let a tiny duration stay if the next 80 ms still has energy
        if (s.end - s.start < (int)(0.025 * w.sr)) {
            int look = std::min(cap, s.start + (int)(0.08 * w.sr));
            float pk = 0; for (int k = s.start; k < look; ++k) pk = std::max(pk, std::fabs(mono[(size_t)k]));
            if (pk > 0.02f) s.end = look;
        }
        s.f = measureHit(mono.data() + s.start, s.end - s.start, w.sr);
        s.lane = classifyHit(s.f);
        out.push_back(s);
    }
    return out;
}

} // namespace cr
