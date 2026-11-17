#pragma once
#include "Dsp.h"
#include <cstdio>
#include <string>

namespace cr {

enum KeyScale { SC_MAJOR, SC_MINOR, SC_CHROMATIC, SC_MAJPENT, SC_MINPENT, SC_COUNT };

inline float midiFromHz(float hz) { hz = std::max(hz, 1e-6f); return 69.f + 12.f * std::log2(hz / 440.f); }
inline float hzFromMidi(float midi) { return 440.f * std::exp2((midi - 69.f) / 12.f); }
inline const char* pitchClassName(int pc) {
    static const char* n[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n[((pc % 12) + 12) % 12];
}

struct NoteCents {
    int midi = 69, pc = 9, octave = 4; float cents = 0, hz = 440; bool valid = false;
};
inline NoteCents noteCentsFromHz(float hz) {
    NoteCents n; if (!(hz > 0) || !std::isfinite(hz)) return n;
    n.hz = hz; n.valid = true;
    float m = midiFromHz(hz); n.midi = (int)std::lround(m); n.cents = (m - (float)n.midi) * 100.f;
    if (n.cents >= 50.f) { n.midi += 1; n.cents -= 100.f; }
    if (n.cents < -50.f) { n.midi -= 1; n.cents += 100.f; }
    n.pc = ((n.midi % 12) + 12) % 12; n.octave = (int)std::floor(n.midi / 12.0) - 1;
    return n;
}
inline std::string formatNoteCents(const NoteCents& n) {
    if (!n.valid) return "-";
    int c = (int)std::lround(n.cents);
    if (c == 0) return std::string(pitchClassName(n.pc)) + std::to_string(n.octave);
    char buf[48]; std::snprintf(buf, sizeof(buf), "%s%d %+d cents", pitchClassName(n.pc), n.octave, c);
    return buf;
}

inline const int* scaleDegrees(int scale, int& count) {
    static const int major[] = {0, 2, 4, 5, 7, 9, 11};
    static const int minor[] = {0, 2, 3, 5, 7, 8, 10};
    static const int chrom[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    static const int majp[] = {0, 2, 4, 7, 9};
    static const int minp[] = {0, 3, 5, 7, 10};
    switch (scale) {
        case SC_MINOR: count = 7; return minor;
        case SC_CHROMATIC: count = 12; return chrom;
        case SC_MAJPENT: count = 5; return majp;
        case SC_MINPENT: count = 5; return minp;
        default: count = 7; return major;
    }
}
inline float snapMidi(float midi, int keyPc, int scale, bool toRoot) {
    keyPc = ((keyPc % 12) + 12) % 12;
    if (toRoot) { float oct = std::round((midi - (float)keyPc) / 12.f); return (float)keyPc + 12.f * oct; }
    int n = 0; const int* deg = scaleDegrees(scale, n);
    float best = midi, bestDist = 1e9f;
    int centre = (int)std::floor(midi / 12.f);
    for (int o = centre - 2; o <= centre + 2; ++o)
        for (int i = 0; i < n; ++i) {
            float cand = (float)(o * 12 + keyPc + deg[i]);
            float d = std::abs(cand - midi);
            if (d < bestDist) { bestDist = d; best = cand; }
        }
    return best;
}
inline float snapHz(float hz, int keyPc, int scale, bool toRoot) { return hzFromMidi(snapMidi(midiFromHz(hz), keyPc, scale, toRoot)); }

// Estimate settled body pitch: skip the first 25 ms (sweep/click), then zero-cross in a later window.
inline float estimateSettledHz(const float* x, int n, double sr) {
    // Skip the click/sweep, then use interpolated first-to-last zero crossings
    // so a window that doesn't land on whole periods cannot bias the count.
    int skip = std::min(n / 3, (int)(0.030 * sr));
    int a = skip, b = std::min(n, skip + (int)(0.160 * sr));
    if (b - a < 16) { a = 0; b = n; }
    std::vector<double> xc;
    for (int i = a + 1; i < b; ++i) {
        float p = x[i - 1], c = x[i];
        if ((c >= 0 && p < 0) || (c < 0 && p >= 0)) {
            float den = c - p;
            double frac = std::abs(den) > 1e-12f ? (0.0 - p) / den : 0.0;
            xc.push_back((i - 1 + frac) / sr);
        }
    }
    if (xc.size() < 3) return 0.f;
    double secs = xc.back() - xc.front();
    if (secs < 1e-4) return 0.f;
    return float(0.5 * (double)(xc.size() - 1) / secs);
}

} // namespace cr
