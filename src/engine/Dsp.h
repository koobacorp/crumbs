#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cr {

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-12f)); }
inline float flush(float x) { return std::abs(x) < 1e-30f ? 0.f : x; }

// ---- saturators (unity at 0, odd-symmetric) ----
inline float softTape(float x, float drive) {
    float d = 1.f + 8.f * drive;
    return std::tanh(x * d) / std::tanh(d);
}
inline float tubeSat(float x, float drive) {
    float d = 1.f + 6.f * drive;
    float y = x * d;
    return y / (1.f + std::fabs(y));
}
inline float hardClip(float x, float drive) {
    float d = 1.f + 10.f * drive;
    return clampf(x * d, -1.f, 1.f);
}
inline float fuzzSat(float x, float drive) {
    float d = 1.f + 14.f * drive;
    float y = std::tanh(x * d);
    return y * y * (y > 0.f ? 1.f : -1.f);
}

enum CrunchType { CR_TAPE = 0, CR_TUBE, CR_CLIP, CR_FUZZ, kCrunchTypes };
inline const char* crunchTypeName(int t) {
    static const char* n[] = {"Tape", "Tube", "Clip", "Fuzz"};
    return n[(t >= 0 && t < kCrunchTypes) ? t : 0];
}
inline float crunchSample(float x, int type, float amt) {
    if (amt <= 1e-5f) return x;
    float w;
    switch (type) {
        case CR_TUBE: w = tubeSat(x, amt); break;
        case CR_CLIP: w = hardClip(x, amt); break;
        case CR_FUZZ: w = fuzzSat(x, amt); break;
        default:      w = softTape(x, amt); break;
    }
    // blend so small amounts stay close to dry (and 0 is exact)
    return lerp(x, w, clampf(amt, 0.f, 1.f));
}

// ---- one-pole ----
struct OnePole {
    float z = 0, a = 0;
    void setHz(float hz, double sr) {
        hz = clampf(hz, 1.f, float(sr * 0.45));
        a = 1.f - std::exp(-2.f * float(M_PI) * hz / float(sr));
    }
    float tick(float x) { z = flush(z + a * (x - z)); return z; }
    float hp(float x) { return x - tick(x); }
    void reset() { z = 0; }
};

struct PeakFollower {
    float env = 0, atk = 0.01f, rel = 0.1f;
    void setMs(float aMs, float rMs, double sr) {
        atk = 1.f - std::exp(-1.f / std::max(1.f, aMs * 0.001f * float(sr)));
        rel = 1.f - std::exp(-1.f / std::max(1.f, rMs * 0.001f * float(sr)));
    }
    float tick(float x) {
        float a = std::fabs(x);
        env = flush(env + ((a > env) ? atk : rel) * (a - env));
        return env;
    }
    void reset() { env = 0; }
};

// Slow program-level auto gain: matches wet loudness to dry without flattening transients.
struct Agc {
    float ein = 0, eout = 0, g = 1, atk = 0.01f, rel = 0.01f, smooth = 0.01f;
    void setMs(float envMs, float smoothMs, double sr) {
        float c = 1.f - std::exp(-1.f / std::max(1.f, envMs * 0.001f * float(sr)));
        atk = c; rel = c * 0.4f;
        smooth = 1.f - std::exp(-1.f / std::max(1.f, smoothMs * 0.001f * float(sr)));
    }
    float tick(float dry, float wet) {
        float ad = std::fabs(dry), aw = std::fabs(wet);
        ein  = flush(ein  + ((ad > ein)  ? atk : rel) * (ad - ein));
        eout = flush(eout + ((aw > eout) ? atk : rel) * (aw - eout));
        float t = (eout > 1e-6f) ? (ein / eout) : 1.f;
        t = clampf(t, 0.25f, 4.f);
        g = flush(g + smooth * (t - g));
        return g;
    }
    void reset() { ein = eout = 0; g = 1; }
};

// RBJ biquad
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    float tick(float x) {
        float y = b0 * x + z1;
        z1 = flush(b1 * x - a1 * y + z2);
        z2 = flush(b2 * x - a2 * y);
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void setLP(float hz, float q, double sr) {
        hz = clampf(hz, 10.f, float(sr * 0.45));
        float w = 2.f * float(M_PI) * hz / float(sr);
        float cw = std::cos(w), sw = std::sin(w);
        float alpha = sw / (2.f * std::max(0.1f, q));
        float a0 = 1.f + alpha;
        b0 = (1.f - cw) * 0.5f / a0; b1 = (1.f - cw) / a0; b2 = b0;
        a1 = (-2.f * cw) / a0; a2 = (1.f - alpha) / a0;
    }
    void setHP(float hz, float q, double sr) {
        hz = clampf(hz, 10.f, float(sr * 0.45));
        float w = 2.f * float(M_PI) * hz / float(sr);
        float cw = std::cos(w), sw = std::sin(w);
        float alpha = sw / (2.f * std::max(0.1f, q));
        float a0 = 1.f + alpha;
        b0 = (1.f + cw) * 0.5f / a0; b1 = -(1.f + cw) / a0; b2 = b0;
        a1 = (-2.f * cw) / a0; a2 = (1.f - alpha) / a0;
    }
    void setLowShelf(float hz, float gainDb, double sr) {
        if (std::fabs(gainDb) < 1e-5f) { identity(); return; }
        hz = clampf(hz, 10.f, float(sr * 0.45));
        float A = std::pow(10.f, gainDb / 40.f);
        float w = 2.f * float(M_PI) * hz / float(sr);
        float cw = std::cos(w), sw = std::sin(w);
        float alpha = sw * 0.5f * std::sqrt(2.f);
        float a0 = (A + 1) + (A - 1) * cw + 2 * std::sqrt(A) * alpha;
        b0 =  (A * ((A + 1) - (A - 1) * cw + 2 * std::sqrt(A) * alpha)) / a0;
        b1 =  (2 * A * ((A - 1) - (A + 1) * cw)) / a0;
        b2 =  (A * ((A + 1) - (A - 1) * cw - 2 * std::sqrt(A) * alpha)) / a0;
        a1 = (-2 * ((A - 1) + (A + 1) * cw)) / a0;
        a2 = ((A + 1) + (A - 1) * cw - 2 * std::sqrt(A) * alpha) / a0;
    }
    void setHighShelf(float hz, float gainDb, double sr) {
        if (std::fabs(gainDb) < 1e-5f) { identity(); return; }
        hz = clampf(hz, 10.f, float(sr * 0.45));
        float A = std::pow(10.f, gainDb / 40.f);
        float w = 2.f * float(M_PI) * hz / float(sr);
        float cw = std::cos(w), sw = std::sin(w);
        float alpha = sw * 0.5f * std::sqrt(2.f);
        float a0 = (A + 1) - (A - 1) * cw + 2 * std::sqrt(A) * alpha;
        b0 =  (A * ((A + 1) + (A - 1) * cw + 2 * std::sqrt(A) * alpha)) / a0;
        b1 = (-2 * A * ((A - 1) + (A + 1) * cw)) / a0;
        b2 =  (A * ((A + 1) + (A - 1) * cw - 2 * std::sqrt(A) * alpha)) / a0;
        a1 = ( 2 * ((A - 1) - (A + 1) * cw)) / a0;
        a2 = ((A + 1) - (A - 1) * cw - 2 * std::sqrt(A) * alpha) / a0;
    }
    void setPeak(float hz, float gainDb, float q, double sr) {
        if (std::fabs(gainDb) < 1e-5f) { identity(); return; }
        hz = clampf(hz, 10.f, float(sr * 0.45));
        float A = std::pow(10.f, gainDb / 40.f);
        float w = 2.f * float(M_PI) * hz / float(sr);
        float cw = std::cos(w), sw = std::sin(w);
        float alpha = sw / (2.f * std::max(0.1f, q));
        float a0 = 1.f + alpha / A;
        b0 = (1.f + alpha * A) / a0;
        b1 = (-2.f * cw) / a0;
        b2 = (1.f - alpha * A) / a0;
        a1 = (-2.f * cw) / a0;
        a2 = (1.f - alpha / A) / a0;
    }
};

// LR4 = two cascaded Butterworth (Q = 1/sqrt(2))
struct LR4 {
    Biquad a, b;
    void setLP(float hz, double sr) { a.setLP(hz, 0.70710678f, sr); b.setLP(hz, 0.70710678f, sr); }
    void setHP(float hz, double sr) { a.setHP(hz, 0.70710678f, sr); b.setHP(hz, 0.70710678f, sr); }
    float tick(float x) { return b.tick(a.tick(x)); }
    void reset() { a.reset(); b.reset(); }
};

// Transparent-until-knee soft ceiling: never exceeds `ceil`, linear below 0.9*ceil.
inline float clipToCeil(float x, float ceil) {
    float c = std::max(1e-6f, ceil);
    float a = std::fabs(x);
    float k = 0.9f * c;
    if (a <= k) return x;
    float span = std::max(1e-8f, c - k);
    float y = k + span * (1.f - std::exp(-(a - k) / span));
    if (y > c) y = c;
    return std::copysign(y, x);
}

// Zero-latency peak controller used when lookahead is off.
struct Ceiling {
    float ceil = dbToGain(-0.1f);
    void setDb(float db) { ceil = dbToGain(clampf(db, -12.f, 0.f)); }
    float tick(float x) { return clipToCeil(x, ceil); }
    void reset() {}
};

// 1.5 ms lookahead limiter: delays the signal, reduces gain so no sample exceeds the ceiling.
struct LookaheadLimiter {
    std::vector<float> L, R;
    int n = 0, w = 0;
    float ceil = dbToGain(-0.1f), gain = 1, rel = 0.01f;
    void prepare(double sr, float ms, float ceilDb) {
        n = std::max(1, (int)std::lround(ms * 0.001 * sr));
        L.assign((size_t)n, 0); R.assign((size_t)n, 0);
        w = 0;
        ceil = dbToGain(clampf(ceilDb, -12.f, 0.f));
        rel = 1.f - std::exp(-1.f / (0.05f * float(sr))); // ~50 ms release
        gain = 1;
    }
    void reset() { std::fill(L.begin(), L.end(), 0); std::fill(R.begin(), R.end(), 0); w = 0; gain = 1; }
    int latency() const { return n; }
    void tick(float inL, float inR, float& outL, float& outR) {
        if (n <= 0) { outL = clipToCeil(inL, ceil); outR = clipToCeil(inR, ceil); return; }
        // Peak of the delayed window plus the new sample (look-ahead of n samples).
        float pk = std::max(std::fabs(inL), std::fabs(inR));
        for (int i = 0; i < n; ++i) {
            float a = std::max(std::fabs(L[(size_t)i]), std::fabs(R[(size_t)i]));
            if (a > pk) pk = a;
        }
        float need = (pk > ceil && pk > 1e-12f) ? (ceil / pk) : 1.f;
        if (need < gain) gain = need;
        else gain = flush(gain + rel * (need - gain));
        outL = clipToCeil(L[(size_t)w] * gain, ceil);
        outR = clipToCeil(R[(size_t)w] * gain, ceil);
        L[(size_t)w] = inL; R[(size_t)w] = inR;
        if (++w >= n) w = 0;
    }
};

// Simple stereo width: width=1 is a bit-exact pass. 0 = mid only.
inline void applyWidth(float& l, float& r, float width) {
    if (std::fabs(width - 1.f) <= 1e-7f) return;
    float mid = 0.5f * (l + r);
    float side = 0.5f * (l - r) * width;
    l = mid + side;
    r = mid - side;
}

// Pan: pan=0 leaves a stereo pair untouched (no hidden 0.707 gain).
inline void applyPan(float& l, float& r, float pan) {
    if (std::fabs(pan) <= 1e-7f) return;
    pan = clampf(pan, -1.f, 1.f);
    float gl = (pan <= 0.f) ? 1.f : (1.f - pan);
    float gr = (pan >= 0.f) ? 1.f : (1.f + pan);
    l *= gl; r *= gr;
}

// Small room: 4 comb + 2 allpass per side. Dry is not delayed. room=0 is a no-op.
struct Room {
    static constexpr int kComb = 4;
    std::vector<float> cL[kComb], cR[kComb], aL[2], aR[2];
    int iCL[kComb] {}, iCR[kComb] {}, iAL[2] {}, iAR[2] {};
    int ready = 0;
    void prepare(double sr) {
        const int baseL[kComb] = {1553, 1613, 1747, 1889};
        const int baseR[kComb] = {1571, 1663, 1777, 1913};
        const int ap[2] = {241, 307};
        float sc = float(sr / 44100.0);
        for (int k = 0; k < kComb; ++k) {
            cL[k].assign((size_t)std::max(16, (int)std::lround(baseL[k] * sc)), 0.f);
            cR[k].assign((size_t)std::max(16, (int)std::lround(baseR[k] * sc)), 0.f);
        }
        for (int k = 0; k < 2; ++k) {
            aL[k].assign((size_t)std::max(8, (int)std::lround(ap[k] * sc)), 0.f);
            aR[k].assign((size_t)std::max(8, (int)std::lround(ap[k] * sc)), 0.f);
        }
        reset();
        ready = 1;
    }
    void reset() {
        for (int k = 0; k < kComb; ++k) {
            std::fill(cL[k].begin(), cL[k].end(), 0.f);
            std::fill(cR[k].begin(), cR[k].end(), 0.f);
            iCL[k] = iCR[k] = 0;
        }
        for (int k = 0; k < 2; ++k) {
            std::fill(aL[k].begin(), aL[k].end(), 0.f);
            std::fill(aR[k].begin(), aR[k].end(), 0.f);
            iAL[k] = iAR[k] = 0;
        }
    }
    static float allpass(std::vector<float>& buf, int& idx, float x, float g) {
        float y = buf[(size_t)idx];
        float out = y - g * x;
        buf[(size_t)idx] = flush(x + g * out);
        if (++idx >= (int)buf.size()) idx = 0;
        return out;
    }
    void tick(float inL, float inR, float amt, float& outL, float& outR) {
        if (amt <= 1e-5f || !ready) { outL = inL; outR = inR; return; }
        float sl = 0, sr_ = 0;
        const float fb = 0.72f;
        for (int k = 0; k < kComb; ++k) {
            int nL = (int)cL[k].size(), nR = (int)cR[k].size();
            float yl = cL[k][(size_t)iCL[k]], yr = cR[k][(size_t)iCR[k]];
            sl += yl; sr_ += yr;
            cL[k][(size_t)iCL[k]] = flush(inL + yl * fb);
            cR[k][(size_t)iCR[k]] = flush(inR + yr * fb);
            iCL[k] = (iCL[k] + 1) % nL;
            iCR[k] = (iCR[k] + 1) % nR;
        }
        sl *= 0.25f; sr_ *= 0.25f;
        sl  = allpass(aL[0], iAL[0], sl,  0.5f);
        sr_ = allpass(aR[0], iAR[0], sr_, 0.5f);
        sl  = allpass(aL[1], iAL[1], sl,  0.5f);
        sr_ = allpass(aR[1], iAR[1], sr_, 0.5f);
        outL = inL + amt * 0.35f * sl;
        outR = inR + amt * 0.35f * sr_;
    }
};

// 2x oversampled crunch. Minimum-phase, designed so an impulse still peaks at sample 0
// (no integer latency). When amt==0 the caller should skip this entirely (bit-exact).
struct CrunchOS {
    OnePole lpL, lpR, dnL, dnR;
    float prevL = 0, prevR = 0;
    void prepare(double sr) {
        // Anti-image / anti-alias around the original Nyquist, at the 2x rate.
        lpL.setHz(float(sr * 0.45), sr * 2); lpR.setHz(float(sr * 0.45), sr * 2);
        dnL.setHz(float(sr * 0.45), sr * 2); dnR.setHz(float(sr * 0.45), sr * 2);
        reset();
    }
    void reset() { lpL.reset(); lpR.reset(); dnL.reset(); dnR.reset(); prevL = prevR = 0; }
    void tick(float l, float r, int type, float amt, float& ol, float& or_) {
        if (amt <= 1e-5f) { ol = l; or_ = r; prevL = l; prevR = r; return; }
        // Linear-interpolate a mid sample (0 extra delay on the first point).
        float midL = lpL.tick(0.5f * (prevL + l));
        float midR = lpR.tick(0.5f * (prevR + r));
        float aL = crunchSample(midL, type, amt);
        float bL = crunchSample(l, type, amt);
        float aR = crunchSample(midR, type, amt);
        float bR = crunchSample(r, type, amt);
        dnL.tick(aL); ol = dnL.tick(bL);
        dnR.tick(aR); or_ = dnR.tick(bR);
        prevL = l; prevR = r;
    }
};

} // namespace cr
