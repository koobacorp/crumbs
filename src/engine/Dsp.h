#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cr {

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float gainToDb(float g) { return 20.f * std::log10(std::max(g, 1e-8f)); }
inline float panL(float pan) { return std::cos(0.25f * float(M_PI) * (pan + 1.f)); } // pan -1..1
inline float panR(float pan) { return std::sin(0.25f * float(M_PI) * (pan + 1.f)); }

inline float softTape(float x, float drive) {
    float d = 1.f + 8.f * drive;
    return std::tanh(x * d) / std::tanh(d);
}
inline float tubeSat(float x, float drive) {
    float d = 1.f + 6.f * drive; float y = x * d;
    return y / (1.f + std::fabs(y));
}
inline float hardClip(float x, float drive) {
    float d = 1.f + 10.f * drive;
    return clampf(x * d, -1.f, 1.f);
}
inline float fuzzSat(float x, float drive) {
    float d = 1.f + 14.f * drive; float y = std::tanh(x * d);
    return y * y * (y > 0 ? 1.f : -1.f);
}

struct OnePole {
    float z = 0, a = 0;
    void setHz(float hz, double sr) { a = 1.f - std::exp(-2.f * float(M_PI) * hz / float(sr)); }
    float tick(float x) { z += a * (x - z); return z; }
    void reset() { z = 0; }
};

struct PeakFollower {
    float env = 0, atk = 0.01f, rel = 0.1f;
    void setMs(float aMs, float rMs, double sr) {
        atk = 1.f - std::exp(-1.f / std::max(1.f, aMs * 0.001f * float(sr)));
        rel = 1.f - std::exp(-1.f / std::max(1.f, rMs * 0.001f * float(sr)));
    }
    float tick(float x) { float a = std::fabs(x); env += (a > env ? atk : rel) * (a - env); return env; }
};

struct Wave {
    std::vector<float> L, R;
    double sr = 44100;
    int n() const { return (int)L.size(); }
    void resize(int samples, double rate) { sr = rate; L.assign((size_t)samples, 0); R.assign((size_t)samples, 0); }
    void clear() { L.clear(); R.clear(); }
    float mono(int i) const {
        if (i < 0 || i >= n()) return 0;
        float r = i < (int)R.size() ? R[(size_t)i] : L[(size_t)i];
        return 0.5f * (L[(size_t)i] + r);
    }
};

inline float hermite(const std::vector<float>& x, float pos) {
    int n = (int)x.size();
    if (n <= 0) return 0;
    int i = (int)std::floor(pos);
    float t = pos - (float)i;
    auto at = [&](int k) { k = clampf((float)k, 0, (float)n - 1); return x[(size_t)k]; };
    float ym1 = at(i - 1), y0 = at(i), y1 = at(i + 1), y2 = at(i + 2);
    float c0 = y0, c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

} // namespace cr
