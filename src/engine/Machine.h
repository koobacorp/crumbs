#pragma once
#include "Detect.h"
#include "Key.h"
#include "Bus.h"
#include <deque>
#include <functional>

namespace cr {

static const uint32_t kPalette[8] = {
    0xFFE85A2A, // warm orange-red
    0xFF5B4FE8, // violet-blue
    0xFF2FA36B, // green
    0xFFE0B400, // golden yellow
    0xFF1F9AA8, // teal
    0xFFC4368A, // magenta
    0xFFC45C2A, // rust
    0xFF3A6EA5  // steel blue
};
inline const char* defaultLaneName(int i) {
    static const char* n[] = {"Kick", "Snare / Clap", "Hats", "Perc", "Lane 5", "Lane 6", "Lane 7", "Lane 8"};
    return n[(i >= 0 && i < 8) ? i : 4];
}

struct Lane {
    std::string name;
    int colour = 0;          // palette index 0-7
    float level = 1, pitch = 0, decay = 1, tone = 0, crunch = 0, pan = 0, room = 0;
    float swing = 0, humanize = 0, ghosts = 0, blend = 0;
    bool mute = false, rebuilt = false;
    // Rebuild knobs (meanings depend on the lane kind)
    float kPunch = 0.5f, kBoom = 0.5f, kTune = 0, kCrunch = 0.2f;
    float kBody = 0.6f, kSnap = 0.5f, kLength = 0.5f, kShimmer = 0.4f;
    int kind = LANE_PERC;    // default rebuild engine / labelling hint
    int midiNote = 36;
};

struct Event {
    int srcStart = 0, srcEnd = 0; // in original wave
    int lane = 0;
    int destStart = 0;            // arranged timeline, samples at original sr
};

struct Snapshot {
    std::vector<Slice> slices;
    std::vector<Event> events;
    std::vector<Lane> lanes;
    int arrangedSamples = 0;
    int selected = 0;
};

enum PlayMode { PLAY_LOOP = 0, PLAY_SAMPLER = 1 };

struct Machine {
    Wave original;
    std::vector<Slice> slices;
    std::vector<Event> events;
    std::vector<Lane> lanes;
    int arrangedSamples = 0;
    int selectedLane = 0;
    int selectedEvent = -1;
    PlayMode playMode = PLAY_LOOP;
    BusSettings bus;
    int keyPc = 0, scale = SC_MAJOR;
    bool keepInKey = false, snapToRoot = false;
    bool abOriginal = false;
    int inferredBeats = 8; // arrangement length in beats (4/4)
    std::deque<Snapshot> undo, redo;

    Machine() { resetLanes(); }

    void resetLanes() {
        lanes.clear();
        const int notes[8] = {36, 38, 42, 39, 41, 43, 45, 47};
        const int kinds[4] = {LANE_KICK, LANE_SNARE, LANE_HATS, LANE_PERC};
        for (int i = 0; i < kDefaultLanes; ++i) {
            Lane L; L.name = defaultLaneName(i); L.colour = i; L.kind = kinds[i]; L.midiNote = notes[i];
            lanes.push_back(L);
        }
    }

    void clearLoop() {
        original.clear(); slices.clear(); events.clear(); arrangedSamples = 0; selectedEvent = -1;
        undo.clear(); redo.clear(); inferredBeats = 8;
    }

    Snapshot snap() const { return Snapshot{slices, events, lanes, arrangedSamples, selectedLane}; }
    void pushUndo() { undo.push_back(snap()); if (undo.size() > 64) undo.pop_front(); redo.clear(); }
    bool canUndo() const { return !undo.empty(); }
    bool canRedo() const { return !redo.empty(); }
    void restore(const Snapshot& s) { slices = s.slices; events = s.events; lanes = s.lanes; arrangedSamples = s.arrangedSamples; selectedLane = s.selected; }
    bool doUndo() { if (undo.empty()) return false; redo.push_back(snap()); restore(undo.back()); undo.pop_back(); return true; }
    bool doRedo() { if (redo.empty()) return false; undo.push_back(snap()); restore(redo.back()); redo.pop_back(); return true; }

    void loadWave(Wave w) {
        clearLoop();
        original = std::move(w);
        if (original.R.size() != original.L.size()) original.R = original.L;
        slices = sliceAndClassify(original);
        events.clear();
        for (auto& s : slices) {
            Event e; e.srcStart = s.start; e.srcEnd = s.end; e.lane = s.lane; e.destStart = s.start;
            events.push_back(e);
        }
        arrangedSamples = original.n();
        inferredBeats = guessBeats(arrangedSamples, original.sr);
        selectedLane = 0; selectedEvent = events.empty() ? -1 : 0;
        undo.clear(); redo.clear();
        bus.applyPreset(BUS_CLEAN);
    }

    static int guessBeats(int samples, double sr) {
        double sec = samples / sr;
        // Prefer 4/4 bar counts that sit near 80–160 BPM.
        int best = 8; double bestErr = 1e9;
        for (int beats : {4, 8, 16, 32, 2, 12, 24}) {
            double bpm = 60.0 * beats / std::max(1e-6, sec);
            double err = std::min(std::abs(bpm - 120), std::abs(bpm - 90));
            if (bpm < 60 || bpm > 200) err += 40;
            if (err < bestErr) { bestErr = err; best = beats; }
        }
        return best;
    }

    bool addLane(const std::string& name = {}) {
        if ((int)lanes.size() >= kMaxLanes) return false;
        pushUndo();
        Lane L; int i = (int)lanes.size();
        L.name = name.empty() ? std::string(defaultLaneName(i)) : name;
        L.colour = i % 8; L.kind = LANE_PERC; L.midiNote = 36 + i * 2;
        lanes.push_back(L);
        selectedLane = i;
        return true;
    }

    bool splitLane(int src, const std::string& aName, const std::string& bName, int bKind = -1) {
        if (src < 0 || src >= (int)lanes.size()) return false;
        if ((int)lanes.size() >= kMaxLanes) return false;
        pushUndo();
        Lane B = lanes[(size_t)src];
        B.name = bName.empty() ? lanes[(size_t)src].name + " B" : bName;
        B.colour = ((lanes[(size_t)src].colour + 4) % 8);
        if (bKind >= 0) B.kind = bKind;
        int bIdx = (int)lanes.size();
        lanes[(size_t)src].name = aName.empty() ? lanes[(size_t)src].name : aName;
        lanes.push_back(B);
        // best-effort: move later / brighter / longer-or-shorter slices
        // Hats split: shorter -> closed (src), longer -> open (B) if kinds hint that.
        for (auto& e : events) if (e.lane == src) {
            int len = e.srcEnd - e.srcStart;
            bool move = false;
            if (lanes[(size_t)src].kind == LANE_HATS)
                move = (len / original.sr) > 0.09; // longer hats -> open
            else if (lanes[(size_t)src].kind == LANE_KICK) {
                // 808s are longer / pitched lower
                std::vector<float> tmp((size_t)std::max(1, len));
                for (int i = 0; i < len && e.srcStart + i < original.n(); ++i) tmp[(size_t)i] = original.mono(e.srcStart + i);
                float hz = estimateSettledHz(tmp.data(), (int)tmp.size(), original.sr);
                move = (len / original.sr) > 0.35 || (hz > 1 && hz < 70);
            } else {
                move = (e.destStart % 2) == 1; // weak fallback
            }
            if (move) e.lane = bIdx;
        }
        selectedLane = src;
        return true;
    }

    bool mergeLanes(int a, int b) {
        if (a < 0 || b < 0 || a >= (int)lanes.size() || b >= (int)lanes.size() || a == b) return false;
        if (a > b) std::swap(a, b);
        pushUndo();
        for (auto& e : events) {
            if (e.lane == b) e.lane = a;
            else if (e.lane > b) e.lane -= 1;
        }
        for (auto& s : slices) {
            if (s.lane == b) s.lane = a;
            else if (s.lane > b) s.lane -= 1;
        }
        lanes.erase(lanes.begin() + b);
        selectedLane = clampf((float)a, 0, (float)lanes.size() - 1);
        return true;
    }

    void renameLane(int i, const std::string& n) { if (i >= 0 && i < (int)lanes.size() && !n.empty()) { pushUndo(); lanes[(size_t)i].name = n; } }
    void recolourLane(int i, int pal) {
        if (i >= 0 && i < (int)lanes.size() && pal >= 0 && pal < 8) { pushUndo(); lanes[(size_t)i].colour = pal; }
    }
    void setLane(int i) { if (i >= 0 && i < (int)lanes.size()) selectedLane = i; }

    void randomizeLane(int i, uint32_t seed = 0) {
        if (i < 0 || i >= (int)lanes.size()) return;
        pushUndo();
        if (!seed) seed = 0xC0FFEEu + (uint32_t)i * 17u + (uint32_t)events.size() * 13u;
        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return float(seed >> 8) / float(0x00FFFFFF); };
        auto& L = lanes[(size_t)i];
        L.pitch = (rnd() - 0.5f) * 7.f;
        L.decay = 0.55f + rnd() * 0.45f;
        L.tone = (rnd() - 0.5f) * 0.8f;
        L.crunch = rnd() * 0.45f;
        L.pan = (rnd() - 0.5f) * 0.7f;
        L.room = rnd() * 0.35f;
        L.swing = rnd() * 0.4f;
        L.humanize = rnd() * 0.35f;
    }

    void copyEvent(int eventIndex, int destStart = -1) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        pushUndo();
        Event n = events[(size_t)eventIndex];
        if (destStart < 0) n.destStart = events[(size_t)eventIndex].destStart + (n.srcEnd - n.srcStart);
        else n.destStart = std::max(0, destStart);
        events.push_back(n);
        selectedEvent = (int)events.size() - 1;
        recomputeLength();
    }

    void fixSort(int eventIndex, int newLane) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        if (newLane < 0 || newLane >= (int)lanes.size()) return;
        pushUndo();
        events[(size_t)eventIndex].lane = newLane;
        selectedLane = newLane; selectedEvent = eventIndex;
    }

    void nudgeEvent(int eventIndex, int deltaSamples) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        pushUndo();
        events[(size_t)eventIndex].destStart = std::max(0, events[(size_t)eventIndex].destStart + deltaSamples);
        recomputeLength();
    }

    void deleteEvent(int eventIndex) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        pushUndo();
        events.erase(events.begin() + eventIndex);
        selectedEvent = events.empty() ? -1 : std::min(eventIndex, (int)events.size() - 1);
        recomputeLength();
    }

    void moveEvent(int eventIndex, int newDestStart) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        pushUndo();
        events[(size_t)eventIndex].destStart = std::max(0, newDestStart);
        recomputeLength();
    }
    void beginGesture() { pushUndo(); }
    void setEventDestNoUndo(int eventIndex, int newDestStart) {
        if (eventIndex < 0 || eventIndex >= (int)events.size()) return;
        events[(size_t)eventIndex].destStart = std::max(0, newDestStart);
        recomputeLength();
    }

    // Duplicate the whole current arrangement onto the end (2 bars -> 4, etc.)
    int duplicateAll() {
        if (events.empty()) return arrangedSamples;
        pushUndo();
        int len = std::max(arrangedSamples, 1);
        int n = (int)events.size();
        for (int i = 0; i < n; ++i) {
            Event e = events[(size_t)i];
            e.destStart += len;
            events.push_back(e);
        }
        arrangedSamples = len * 2;
        return arrangedSamples;
    }

    // Duplicate a time region [start, end) to dest (or to the end if dest < 0).
    void duplicateRange(int start, int end, int dest = -1) {
        if (end <= start) return;
        pushUndo();
        int width = end - start;
        if (dest < 0) dest = arrangedSamples;
        std::vector<Event> add;
        for (auto& e : events) if (e.destStart >= start && e.destStart < end) {
            Event n = e; n.destStart = dest + (e.destStart - start); add.push_back(n);
        }
        events.insert(events.end(), add.begin(), add.end());
        arrangedSamples = std::max(arrangedSamples, dest + width);
    }

    void recomputeLength() {
        int m = 0;
        for (auto& e : events) m = std::max(m, e.destStart + (e.srcEnd - e.srcStart));
        arrangedSamples = std::max(m, arrangedSamples);
    }

    // ---- Rebuild ----
    void rebuildLane(int li) {
        if (li < 0 || li >= (int)lanes.size()) return;
        pushUndo();
        // Analyse all events in this lane for typical pitch / decay / brightness.
        int nEv = 0; double dur = 0, thump = 0, snap = 0, sizzle = 0, hzAcc = 0; int hzN = 0;
        for (auto& e : events) if (e.lane == li) {
            int len = std::max(1, e.srcEnd - e.srcStart);
            std::vector<float> tmp((size_t)len);
            for (int i = 0; i < len && e.srcStart + i < original.n(); ++i) tmp[(size_t)i] = original.mono(e.srcStart + i);
            auto f = measureHit(tmp.data(), len, original.sr);
            dur += f.duration; thump += f.thump; snap += f.snap; sizzle += f.sizzle;
            float hz = estimateSettledHz(tmp.data(), len, original.sr);
            if (hz > 20 && hz < 2000) { hzAcc += hz; ++hzN; }
            ++nEv;
        }
        auto& L = lanes[(size_t)li];
        if (nEv > 0) {
            dur /= nEv; thump /= nEv; snap /= nEv; sizzle /= nEv;
            L.kBoom = clampf(float(dur / 0.6), 0, 1);
            L.kLength = clampf(float(dur / 0.4), 0, 1);
            L.kPunch = clampf(float(snap * 1.2), 0, 1);
            L.kSnap = clampf(float(snap), 0, 1);
            L.kBody = clampf(float(0.3 + 0.7 * thump), 0, 1);
            L.kShimmer = clampf(float(sizzle), 0, 1);
            L.kCrunch = 0.15f;
            if (hzN > 0) {
                float hz = float(hzAcc / hzN);
                // Tune relative to a default (50 Hz kick, 185 Hz snare, 8000 Hz hats is not useful)
                float ref = L.kind == LANE_KICK ? 50.f : (L.kind == LANE_SNARE ? 185.f : 110.f);
                L.kTune = clampf(12.f * std::log2(hz / ref), -12, 12);
            }
        }
        L.rebuilt = true;
        if (L.blend < 0.05f) L.blend = 0.5f;
    }

    // Render one synth hit of `length` samples for a rebuilt lane.
    void renderSynthHit(const Lane& L, float* out, int length, double sr) const {
        std::fill(out, out + length, 0.f);
        int kind = L.kind;
        if (kind == LANE_KICK) {
            float f0 = 50.f * std::exp2(L.kTune / 12.f);
            if (keepInKey) f0 = snapHz(f0, keyPc, scale, snapToRoot);
            float boom = lerp(0.12f, 1.4f, L.kBoom);
            float punch = L.kPunch;
            double ph = 0, phs = 0;
            for (int i = 0; i < length; ++i) {
                float t = float(i) / float(sr);
                float sweep = punch * 24.f * std::exp(-t / 0.030f); // semitones above
                float f = f0 * std::exp2(sweep / 12.f);
                ph += 2 * M_PI * f / sr;
                float env = std::exp(-t / std::max(0.05f, boom));
                float click = punch * std::exp(-t / 0.004f) * ((i % 3) ? 0.4f : 1.f);
                float s = std::sin(ph) * env + 0.25f * click;
                s = crunchSample(s, CR_TAPE, L.kCrunch);
                out[i] = s * 0.9f;
                (void)phs;
            }
        } else if (kind == LANE_SNARE) {
            float f0 = 185.f * std::exp2(L.kTune / 12.f);
            float body = L.kBody, snap = L.kSnap, dec = lerp(0.06f, 0.45f, L.kLength);
            uint32_t rng = 0xA341316Cu;
            for (int i = 0; i < length; ++i) {
                float t = float(i) / float(sr);
                rng = rng * 1664525u + 1013904223u;
                float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
                float benv = std::exp(-t / 0.08f);
                float nenv = std::exp(-t / dec);
                float crk = snap * std::exp(-t / 0.006f) * noise;
                float s = body * 0.5f * std::sin(2 * M_PI * f0 * t) * benv + 0.55f * noise * nenv + crk;
                s = crunchSample(s, CR_TAPE, L.kCrunch);
                out[i] = s * 0.8f;
            }
        } else if (kind == LANE_HATS) {
            float dec = lerp(0.03f, 0.35f, L.kLength);
            float shimmer = L.kShimmer;
            uint32_t rng = 0xC001D00Du;
            float hp = 0, bp = 0;
            float hpA = 1.f - std::exp(-2.f * float(M_PI) * lerp(4000, 9000, shimmer) / float(sr));
            for (int i = 0; i < length; ++i) {
                float t = float(i) / float(sr);
                rng = rng * 1664525u + 1013904223u;
                float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
                hp += hpA * (noise - hp); float y = noise - hp;
                bp += 0.2f * (y - bp);
                float env = std::exp(-t / dec);
                out[i] = crunchSample((y + shimmer * bp) * env * 0.7f, CR_TAPE, L.kCrunch);
            }
        } else {
            float f0 = 110.f * std::exp2(L.kTune / 12.f);
            if (keepInKey) f0 = snapHz(f0, keyPc, scale, snapToRoot);
            float dec = lerp(0.08f, 0.6f, L.kLength);
            uint32_t rng = 0x51EDu;
            for (int i = 0; i < length; ++i) {
                float t = float(i) / float(sr);
                rng = rng * 1664525u + 1013904223u;
                float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
                float env = std::exp(-t / dec);
                out[i] = crunchSample((L.kBody * std::sin(2 * M_PI * f0 * t) + 0.2f * noise) * env, CR_TAPE, L.kCrunch);
            }
        }
    }

    // Pitch of a source slice (for key display). For kicks/toms, settled body.
    float eventSettledHz(const Event& e) const {
        int len = std::max(1, e.srcEnd - e.srcStart);
        std::vector<float> tmp((size_t)len);
        for (int i = 0; i < len && e.srcStart + i < original.n(); ++i) tmp[(size_t)i] = original.mono(e.srcStart + i);
        return estimateSettledHz(tmp.data(), len, original.sr);
    }

    float laneDisplayHz(int li) const {
        for (auto& e : events) if (e.lane == li) {
            float hz = eventSettledHz(e);
            if (hz > 20) {
                hz *= std::exp2(lanes[(size_t)li].pitch / 12.f);
                if (lanes[(size_t)li].rebuilt) hz *= std::exp2(lanes[(size_t)li].kTune / 12.f);
                if (keepInKey && (lanes[(size_t)li].kind == LANE_KICK || lanes[(size_t)li].kind == LANE_PERC))
                    hz = snapHz(hz, keyPc, scale, snapToRoot);
                return hz;
            }
        }
        return 0;
    }

    // ---- Render arranged audio (offline, for tests, export, and the process cache) ----
    Wave render(bool applyTheBus = true, bool forceOriginal = false) const {
        Wave out; int N = std::max(arrangedSamples, 1); out.resize(N, original.sr);
        if (forceOriginal || abOriginal) {
            int n = std::min(N, original.n());
            for (int i = 0; i < n; ++i) { out.L[(size_t)i] = original.L[(size_t)i]; out.R[(size_t)i] = original.R.empty() ? original.L[(size_t)i] : original.R[(size_t)i]; }
            return out;
        }
        std::vector<float> kickEnv((size_t)N, 0);
        for (auto& e : events) {
            if (e.lane < 0 || e.lane >= (int)lanes.size()) continue;
            const auto& L = lanes[(size_t)e.lane];
            if (L.mute) continue;
            int srcLen = std::max(1, e.srcEnd - e.srcStart);
            // higher pitch = faster read = higher frequency and a shorter hit
            float rate = std::exp2(L.pitch / 12.f);
            if (keepInKey && (L.kind == LANE_KICK || L.kind == LANE_PERC)) {
                float hz = eventSettledHz(e);
                if (hz > 20) {
                    float target = snapHz(hz * std::exp2(L.pitch / 12.f), keyPc, scale, snapToRoot);
                    rate = target / hz;
                }
            }
            int dstLen = std::max(8, (int)std::lround(srcLen / std::max(0.05f, rate)));
            if (L.decay < 0.999f) dstLen = std::max(8, (int)(dstLen * clampf(L.decay, 0.05f, 1.f)));
            std::vector<float> synth;
            if (L.rebuilt && L.blend > 0.001f) {
                synth.resize((size_t)dstLen);
                renderSynthHit(L, synth.data(), dstLen, original.sr);
            }
            float g = L.level * (L.mute ? 0.f : 1.f);
            int dest = e.destStart;
            if (L.swing > 1e-5f && inferredBeats > 0 && arrangedSamples > 0) {
                double sp16 = arrangedSamples / (inferredBeats * 4.0);
                int i16 = (int)std::floor(dest / std::max(1.0, sp16) + 1e-6);
                if (i16 % 2 == 1) dest += (int)std::lround(L.swing * 0.5 * sp16);
            }
            if (L.humanize > 1e-5f) {
                uint32_t h = 0x9E3779B9u ^ (uint32_t)e.destStart * 747796405u + (uint32_t)e.lane * 1013904223u;
                h = h * 1664525u + 1013904223u;
                float r = (float((h >> 9) & 0xFFFF) / 32767.f) * 2.f - 1.f;
                dest += (int)std::lround(r * L.humanize * 0.012f * original.sr);
                h = h * 1664525u + 1013904223u;
                float r2 = (float((h >> 9) & 0xFFFF) / 32767.f) * 2.f - 1.f;
                g *= 1.f + 0.15f * L.humanize * r2;
            }
            dest = std::max(0, dest);
            float gL = g * panL(L.pan), gR = g * panR(L.pan);
            int roomDelay = (int)std::lround(0.022 * original.sr);
            for (int i = 0; i < dstLen; ++i) {
                int di = dest + i;
                if (di < 0 || di >= N) continue;
                float pos = e.srcStart + i * rate;
                float sL = hermite(original.L, pos);
                float sR = original.R.empty() ? sL : hermite(original.R, pos);
                // tone: tilt
                if (L.tone != 0) {
                    // cheap tilt: mix with a one-sample difference
                    float hp = sL - (i > 0 ? hermite(original.L, pos - 1) : sL);
                    sL += L.tone * 0.5f * hp; sR += L.tone * 0.5f * hp;
                }
                if (L.crunch > 0) { sL = crunchSample(sL, CR_TAPE, L.crunch); sR = crunchSample(sR, CR_TAPE, L.crunch); }
                if (!synth.empty() && L.blend > 0) {
                    sL = lerp(sL, synth[(size_t)i], L.blend);
                    sR = lerp(sR, synth[(size_t)i], L.blend);
                }
                // decay envelope
                float env = 1.f;
                if (L.decay < 0.999f) env = std::exp(-float(i) / (float(dstLen) * 0.45f));
                sL *= env; sR *= env;
                out.L[(size_t)di] += sL * gL; out.R[(size_t)di] += sR * gR;
                if (L.room > 1e-5f) {
                    int rj = di + roomDelay;
                    if (rj < N) {
                        float wet = L.room * 0.45f;
                        out.L[(size_t)rj] += sL * gL * wet;
                        out.R[(size_t)rj] += sR * gR * wet * 0.85f;
                    }
                }
                if (L.kind == LANE_KICK) kickEnv[(size_t)di] = std::max(kickEnv[(size_t)di], std::fabs(0.5f * (sL + sR)));
            }
            // Ghost hits: a quieter copy of this event halfway to the next same-lane event.
            if (L.ghosts > 1e-5f && (L.kind == LANE_SNARE || L.kind == LANE_HATS)) {
                int next = N;
                for (auto& o : events) if (o.lane == e.lane && o.destStart > e.destStart)
                    next = std::min(next, o.destStart);
                if (next - e.destStart > (int)(0.08 * original.sr)) {
                    int mid = e.destStart + (next - e.destStart) / 2;
                    float gg = g * 0.35f * L.ghosts;
                    float ggL = gg * panL(L.pan), ggR = gg * panR(L.pan);
                    int gLen = std::min(dstLen, (int)(0.12 * original.sr));
                    for (int i = 0; i < gLen; ++i) {
                        int di = mid + i; if (di < 0 || di >= N) continue;
                        float pos = e.srcStart + i * rate;
                        out.L[(size_t)di] += hermite(original.L, pos) * ggL;
                        out.R[(size_t)di] += hermite(original.R.empty() ? original.L : original.R, pos) * ggR;
                    }
                }
            }
        }
        if (applyTheBus) applyBus(out.L.data(), out.R.data(), N, original.sr, bus, kickEnv.data());
        // Always-on safety ceiling. Clean stays a true bypass for signals already under it.
        for (int i = 0; i < N; ++i) {
            out.L[(size_t)i] = clampf(out.L[(size_t)i], -0.98f, 0.98f);
            out.R[(size_t)i] = clampf(out.R[(size_t)i], -0.98f, 0.98f);
        }
        return out;
    }

    // Lane isolation render (for drag-out of one lane)
    Wave renderLane(int li) const {
        Machine m = *this;
        for (int i = 0; i < (int)m.lanes.size(); ++i) m.lanes[(size_t)i].mute = (i != li);
        m.bus.applyPreset(BUS_CLEAN);
        return m.render(false, false);
    }

    // Simple SMF MIDI (format 0), one note per event, lane's midiNote, 480 PPQ.
    std::vector<uint8_t> exportMidi() const {
        auto writeVar = [](std::vector<uint8_t>& o, unsigned v) {
            uint8_t buf[4]; int n = 0; buf[n++] = v & 0x7f; v >>= 7;
            while (v) { buf[n++] = (uint8_t)((v & 0x7f) | 0x80); v >>= 7; }
            while (n--) o.push_back(buf[n]);
        };
        double sr = original.sr > 0 ? original.sr : 44100;
        double bpm = 60.0 * inferredBeats / std::max(1e-6, arrangedSamples / sr);
        int us = (int)std::lround(60e6 / std::max(1.0, bpm));
        std::vector<uint8_t> ev;
        // tempo
        ev.push_back(0); ev.push_back(0xFF); ev.push_back(0x51); ev.push_back(0x03);
        ev.push_back((uint8_t)((us >> 16) & 0xFF)); ev.push_back((uint8_t)((us >> 8) & 0xFF)); ev.push_back((uint8_t)(us & 0xFF));
        int ppq = 480;
        struct Mid { int tick; int lane; int on; };
        std::vector<Mid> ms;
        for (auto& e : events) {
            int tick = (int)std::lround((e.destStart / sr) * (bpm / 60.0) * ppq);
            int dur = std::max(30, (int)std::lround(((e.srcEnd - e.srcStart) / sr) * (bpm / 60.0) * ppq));
            ms.push_back({tick, e.lane, 1});
            ms.push_back({tick + dur, e.lane, 0});
        }
        std::sort(ms.begin(), ms.end(), [](const Mid& a, const Mid& b) { return a.tick < b.tick || (a.tick == b.tick && a.on < b.on); });
        int last = 0;
        for (auto& m : ms) {
            int dt = std::max(0, m.tick - last); last = m.tick;
            writeVar(ev, (unsigned)dt);
            int note = 36;
            if (m.lane >= 0 && m.lane < (int)lanes.size()) note = lanes[(size_t)m.lane].midiNote;
            ev.push_back((uint8_t)(m.on ? 0x99 : 0x89)); ev.push_back((uint8_t)clampf((float)note, 0, 127)); ev.push_back((uint8_t)(m.on ? 100 : 0));
        }
        ev.push_back(0); ev.push_back(0xFF); ev.push_back(0x2F); ev.push_back(0x00);
        std::vector<uint8_t> out;
        auto u32 = [&](uint32_t v) { out.push_back(v >> 24); out.push_back(v >> 16); out.push_back(v >> 8); out.push_back(v); };
        auto u16 = [&](uint16_t v) { out.push_back(v >> 8); out.push_back(v); };
        out.insert(out.end(), {'M','T','h','d'}); u32(6); u16(0); u16(1); u16((uint16_t)ppq);
        out.insert(out.end(), {'M','T','r','k'}); u32((uint32_t)ev.size()); out.insert(out.end(), ev.begin(), ev.end());
        return out;
    }

    // Playback helper: interpolate arranged render at a host-sync position.
    // ppq is quarter-notes from the loop start; we map inferredBeats quarters to arrangedSamples.
    void mixAt(float* L, float* R, int n, double hostBpm, double ppq, const Wave& cache) const {
        if (cache.n() <= 0 || arrangedSamples <= 0) { for (int i = 0; i < n; ++i) L[i] = R[i] = 0; return; }
        double beats = inferredBeats;
        double loopSec = 60.0 * beats / std::max(1.0, hostBpm);
        double pos0 = std::fmod(ppq, beats); if (pos0 < 0) pos0 += beats;
        double sampPerBeat = arrangedSamples / beats;
        for (int i = 0; i < n; ++i) {
            double p = pos0 + (i / (cache.sr > 0 ? cache.sr : 44100.0)) * (hostBpm / 60.0);
            p = std::fmod(p, beats); if (p < 0) p += beats;
            float sPos = float(p * sampPerBeat);
            L[i] = hermite(cache.L, sPos);
            R[i] = hermite(cache.R, sPos);
        }
        (void)loopSec;
    }
};

// -------- helpers to build synthetic test loops --------
inline void addKick(Wave& w, int at, float hz = 50, float amp = 0.9f) {
    int n = w.n(); double sr = w.sr; float ph = 0;
    for (int i = 0; i < (int)(0.4 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        float f = hz * std::exp2(18.f * std::exp(-t / 0.008f) / 12.f);
        float env = std::exp(-t / 0.28f);
        ph += 2.f * float(M_PI) * f / float(sr);
        float s = amp * env * std::sin(ph);
        w.L[(size_t)(at + i)] += s; w.R[(size_t)(at + i)] += s;
    }
}
inline void addSnare(Wave& w, int at, float amp = 0.7f) {
    int n = w.n(); double sr = w.sr; uint32_t rng = 0x12345678u + (uint32_t)at;
    for (int i = 0; i < (int)(0.22 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        rng = rng * 1664525u + 1013904223u;
        float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
        float body = 0.45f * std::sin(2 * float(M_PI) * 200.f * t) * std::exp(-t / 0.05f);
        float s = amp * (body + 0.7f * noise * std::exp(-t / 0.09f) + 0.5f * noise * std::exp(-t / 0.008f));
        w.L[(size_t)(at + i)] += s; w.R[(size_t)(at + i)] += s * 0.95f;
    }
}
inline void addHat(Wave& w, int at, float amp = 0.35f, float dec = 0.05f) {
    int n = w.n(); double sr = w.sr; uint32_t rng = 0xC0FFEEu + (uint32_t)at * 17;
    float hp = 0, a = 1.f - std::exp(-2.f * float(M_PI) * 7000.f / float(sr));
    for (int i = 0; i < (int)(0.2 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        rng = rng * 1664525u + 1013904223u;
        float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
        hp += a * (noise - hp);
        float s = amp * (noise - hp) * std::exp(-t / dec);
        w.L[(size_t)(at + i)] += s; w.R[(size_t)(at + i)] += s;
    }
}
inline void addPerc(Wave& w, int at, float hz = 220, float amp = 0.5f) {
    int n = w.n(); double sr = w.sr; float ph = 0;
    for (int i = 0; i < (int)(0.15 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        ph += 2.f * float(M_PI) * hz / float(sr);
        float s = amp * std::sin(ph) * std::exp(-t / 0.08f);
        w.L[(size_t)(at + i)] += s; w.R[(size_t)(at + i)] += s;
    }
}

} // namespace cr
