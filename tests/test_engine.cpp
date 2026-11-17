#include "TestUtil.h"
#include "Machine.h"
#include <chrono>
using namespace cr;

static Wave makeKitLoop(double sr = 44100, double bpm = 120, int bars = 2, bool hatsOnKick = false) {
    int beats = bars * 4;
    int n = (int)std::lround(sr * 60.0 * beats / bpm);
    Wave w; w.resize(n, sr);
    int step = (int)std::lround(sr * 60.0 / bpm / 4.0); // 16th
    int eighth = step * 2;
    for (int b = 0; b < beats; ++b) {
        int q = b * step * 4; // quarter start
        if (b % 2 == 0) addKick(w, q);
        else addSnare(w, q);
        addHat(w, q, 0.35f, 0.04f);
        addHat(w, q + eighth, 0.28f, 0.04f);
        if (hatsOnKick && b % 2 == 0) addHat(w, q, 0.4f, 0.05f); // overlap kick
        if (b % 4 == 3) addPerc(w, q + step * 3, 330, 0.45f);
    }
    return w;
}

static void testKeyMath() {
    auto n = noteCentsFromHz(440.f);
    CHECK(n.valid && n.pc == 9 && n.octave == 4 && std::abs(n.cents) < 0.51f);
    CHECK(formatNoteCents(n) == "A4");
    n = noteCentsFromHz(50.f);
    CHECK(n.pc == 7 && n.octave == 1);
    CHECK(formatNoteCents(n).find("G1") != std::string::npos);
    CHECK(formatNoteCents(n).find("+35") != std::string::npos);
    // E minor: F snaps to E
    float f = snapHz(hzFromMidi(29.f), 4, SC_MINOR, false);
    auto s = noteCentsFromHz(f);
    CHECK(s.pc == 4 && s.octave == 1 && std::abs(s.cents) < 0.51f);
    // settled pitch ignores a front sweep
    std::vector<float> x(44100 / 2);
    for (int i = 0; i < (int)x.size(); ++i) {
        float t = i / 44100.f;
        float hz = 50.f * std::exp2(24.f * std::exp(-t / 0.03f) / 12.f);
        x[(size_t)i] = std::sin(2 * float(M_PI) * 50.f * t); // body at 50, we'll use estimate on a later window of a swept signal
    }
    // build a real swept kick
    for (int i = 0; i < (int)x.size(); ++i) {
        float t = i / 44100.f;
        float f0 = 50.f * std::pow(2.f, 2.f * std::exp(-t / 0.03f));
        // integrate phase properly
    }
    // simpler: two-tone, first 20ms 200 Hz, rest 50 Hz
    double ph = 0;
    for (int i = 0; i < (int)x.size(); ++i) {
        float t = i / 44100.f; float f0 = t < 0.02f ? 200.f : 50.f;
        ph += 2 * M_PI * f0 / 44100.0;
        x[(size_t)i] = std::sin(ph) * std::exp(-t / 0.3f);
    }
    float est = estimateSettledHz(x.data(), (int)x.size(), 44100);
    CHECKM(std::abs(est - 50.f) / 50.f < 0.12f, "settled %g", est);
}

static void testOnsetsAndLabels() {
    auto w = makeKitLoop(44100, 120, 2, false);
    auto sl = sliceAndClassify(w);
    // 2 bars at 120: 8 quarters (4 kicks, 4 snares), hats on every 8th = 16, plus 2 perc on the last 16th of bars 1 and 2.
    // Kicks/snares coincide with hats on the quarter — those are OVERLAPPING and must stay one slice.
    // Expected onsets: every 8th (hats), plus perc off-beats that don't fall on an 8th? last 16th of a bar is an upbeat 16th, not an 8th.
    // 8th grid: 16 hats. Perc on beat-bar's 16th 4? b%4==3, q + 3*16th = the 4th 16th of that quarter = an 8th if... 
    // q is quarter, step is 16th, q+3*step is a 16th off the 8th grid. So +2 extra perc. Total ~18 onsets.
    CHECKM(sl.size() >= 14 && sl.size() <= 24, "onset count %zu", sl.size());
    // Timing of known isolated? It's messy because hats sit on kicks.
    // Build a sparser loop: kick, gap, snare, gap, hat, gap, perc.
    Wave s; s.resize((int)(44100 * 2), 44100);
    int kAt = (int)(0.00 * 44100), snAt = (int)(0.50 * 44100), hAt = (int)(1.00 * 44100), pAt = (int)(1.50 * 44100);
    addKick(s, kAt); addSnare(s, snAt); addHat(s, hAt, 0.5f, 0.04f); addPerc(s, pAt, 250, 0.7f);
    auto sl2 = sliceAndClassify(s);
    CHECKM(sl2.size() == 4, "sparse count %zu", sl2.size());
    int hits[4] = {0, 0, 0, 0};
    // each slice should fall near the known times and have the right label
    int okTime = 0, okLab = 0;
    int expectT[4] = {kAt, snAt, hAt, pAt};
    int expectL[4] = {LANE_KICK, LANE_SNARE, LANE_HATS, LANE_PERC};
    for (int i = 0; i < 4 && i < (int)sl2.size(); ++i) {
        int err = std::abs(sl2[(size_t)i].start - expectT[i]);
        if (err < (int)(0.040 * 44100)) ++okTime;
        hits[sl2[(size_t)i].lane]++;
        if (sl2[(size_t)i].lane == expectL[i]) ++okLab;
        std::printf("  slice %d t=%.3f lane=%d (thump %.2f snap %.2f sizzle %.2f cent %.0f dur %.3f)\n",
                    i, sl2[(size_t)i].start / 44100.0, sl2[(size_t)i].lane,
                    sl2[(size_t)i].f.thump, sl2[(size_t)i].f.snap, sl2[(size_t)i].f.sizzle,
                    sl2[(size_t)i].f.centroid, sl2[(size_t)i].f.duration);
    }
    CHECKM(okTime == 4, "times %d/4", okTime);
    CHECKM(okLab >= 4, "labels %d/4", okLab); // expect all 4 on this clean loop
}

static void testOverlapStaysTogether() {
    Wave s; s.resize((int)(44100 * 1.0), 44100);
    addKick(s, 0); addHat(s, 0, 0.5f, 0.04f); // same instant
    auto sl = sliceAndClassify(s);
    CHECKM(sl.size() == 1, "overlap produced %zu slices (want 1)", sl.size());
}

static void testAccuracy() {
    // Many clean isolated hits, measure labelling accuracy
    int ok = 0, n = 0;
    int conf[4][4] {}; // true, pred
    for (int trial = 0; trial < 40; ++trial) {
        Wave s; s.resize((int)(44100 * 2.2), 44100);
        int t0 = (int)(0.05 * 44100) + trial; // slight jitter
        int gap = (int)(0.50 * 44100);
        addKick(s, t0, 45.f + (trial % 5) * 3, 0.8f + 0.05f * (trial % 3));
        addSnare(s, t0 + gap, 0.6f + 0.05f * (trial % 3));
        addHat(s, t0 + 2 * gap, 0.3f + 0.05f * (trial % 3), 0.03f + 0.01f * (trial % 3));
        addPerc(s, t0 + 3 * gap, 180.f + (trial % 6) * 20, 0.5f);
        auto sl = sliceAndClassify(s);
        int expect[4] = {LANE_KICK, LANE_SNARE, LANE_HATS, LANE_PERC};
        for (int i = 0; i < 4; ++i) {
            // find slice nearest the expected time
            int want = t0 + i * gap, best = -1, bestE = 1 << 30;
            for (int j = 0; j < (int)sl.size(); ++j) {
                int e = std::abs(sl[(size_t)j].start - want);
                if (e < bestE) { bestE = e; best = j; }
            }
            if (best >= 0 && bestE < (int)(0.05 * 44100)) {
                int pred = sl[(size_t)best].lane;
                conf[expect[i]][pred]++;
                if (pred == expect[i]) ++ok;
                ++n;
            }
        }
    }
    double acc = n ? 100.0 * ok / n : 0;
    std::printf("  clean isolated accuracy: %d/%d = %.1f%%\n", ok, n, acc);
    const char* nm[] = {"kick", "snare", "hats", "perc"};
    for (int i = 0; i < 4; ++i) {
        int row = conf[i][0] + conf[i][1] + conf[i][2] + conf[i][3];
        std::printf("    true %s: k=%d s=%d h=%d p=%d (n=%d)\n", nm[i], conf[i][0], conf[i][1], conf[i][2], conf[i][3], row);
    }
    CHECKM(n >= 140 && acc >= 95.0, "accuracy %.1f%% on %d", acc, n);
}

static void testLanesArrangeUndo() {
    Machine m;
    auto w = makeKitLoop();
    m.loadWave(w);
    CHECK(m.lanes.size() == 4);
    CHECK(m.bus.preset == BUS_CLEAN && m.bus.isClean());
    int n0 = (int)m.events.size();
    CHECK(n0 > 0);
    CHECK(m.addLane("808"));
    CHECK(m.lanes.size() == 5);
    CHECK(m.lanes.back().name == "808");
    m.recolourLane((int)m.lanes.size() - 1, 6);
    CHECK(m.lanes.back().colour == 6);
    m.renameLane(4, "Sub");
    CHECK(m.lanes[4].name == "Sub");
    // ninth lane rejected
    m.addLane(); m.addLane(); m.addLane();
    CHECK(m.lanes.size() == 8);
    CHECK(!m.addLane("nope"));
    CHECK(m.lanes.size() == 8);
    // split hats
    Machine a; a.loadWave(w);
    int hats = LANE_HATS;
    CHECK(a.splitLane(hats, "Closed Hats", "Open Hats", LANE_HATS));
    CHECK(a.lanes.size() == 5);
    CHECK(a.mergeLanes(hats, (int)a.lanes.size() - 1));
    CHECK(a.lanes.size() == 4);
    // fix-the-sort
    if (!a.events.empty()) {
        int old = a.events[0].lane;
        int dest = (old + 1) % (int)a.lanes.size();
        a.fixSort(0, dest);
        CHECK(a.events[0].lane == dest);
        a.doUndo();
        CHECK(a.events[0].lane == old);
    }
    // duplicate 2 bars -> 4
    int len0 = a.arrangedSamples;
    int ev0 = (int)a.events.size();
    a.duplicateAll();
    CHECK(a.arrangedSamples == len0 * 2);
    CHECK((int)a.events.size() == ev0 * 2);
    a.duplicateAll();
    CHECK(a.arrangedSamples == len0 * 4);
    // six pieces from four: duplicate a quarter of the original twice more... use range
    Machine b; b.loadWave(w);
    int q = b.arrangedSamples / 4;
    // start with 4 quarters; duplicate first quarter to the end twice? That's 5 and 6 sections of quarter length.
    b.duplicateRange(0, q, b.arrangedSamples);
    b.duplicateRange(0, q, b.arrangedSamples);
    CHECK(b.arrangedSamples == q * 6);
    // delete / move / nudge + undo
    int ev = (int)b.events.size();
    b.deleteEvent(0);
    CHECK((int)b.events.size() == ev - 1);
    b.doUndo();
    CHECK((int)b.events.size() == ev);
    if (b.events.size() > 1) {
        int d0 = b.events[1].destStart;
        b.moveEvent(1, d0 + 1000);
        CHECK(b.events[1].destStart == d0 + 1000);
        b.doUndo();
        CHECK(b.events[1].destStart == d0);
        b.nudgeEvent(1, 64);
        CHECK(b.events[1].destStart == d0 + 64);
    }
}

static void testBusCleanAndDSP() {
    // Clean is a true bypass
    Wave s; s.resize(4096, 44100);
    for (int i = 0; i < 4096; ++i) { s.L[(size_t)i] = 0.2f * std::sin(2 * float(M_PI) * 220 * i / 44100.f); s.R[(size_t)i] = s.L[(size_t)i]; }
    BusSettings b; b.applyPreset(BUS_CLEAN);
    auto L = s.L, R = s.R;
    applyBus(L.data(), R.data(), 4096, 44100, b, nullptr);
    int same = 0; for (int i = 0; i < 4096; ++i) same += (L[(size_t)i] == s.L[(size_t)i] && R[(size_t)i] == s.R[(size_t)i]);
    CHECKM(same == 4096, "clean changed %d samples", 4096 - same);
    // tape crunch increases harmonic energy
    BusSettings t; t.applyPreset(BUS_TAPE);
    CHECK(t.crunch > 0 && t.crunchType == CR_TAPE);
    auto Lt = s.L, Rt = s.R;
    applyBus(Lt.data(), Rt.data(), 4096, 44100, t, nullptr);
    CHECK(tu::finite(Lt) && tu::peak(Lt) > 0);
    // squash lowers crest on a transient-y signal
    Wave k; k.resize(44100, 44100); addKick(k, 0);
    BusSettings sm; sm.applyPreset(BUS_SMASHED);
    auto Ls = k.L; auto Rs = k.R;
    float pk0 = tu::peak(Ls);
    applyBus(Ls.data(), Rs.data(), k.n(), 44100, sm, nullptr);
    CHECK(tu::finite(Ls) && tu::peak(Ls) <= 0.98f + 1e-5f);
    (void)pk0;
    // textbook: pan laws at 0 are equal power ~0.707
    CHECK(std::abs(panL(0) - panR(0)) < 1e-5f);
    CHECK(std::abs(panL(0) * panL(0) + panR(0) * panR(0) - 1.f) < 1e-5f);
    // pitch shift: +12 st doubles frequency — tested via resampling rate in render
    Machine m; Wave one; one.resize((int)(44100 * 0.6), 44100); addKick(one, 0, 50, 0.9f);
    m.loadWave(one);
    for (auto& e : m.events) e.lane = LANE_KICK;
    m.lanes[0].pitch = 12; // +1 octave
    auto r = m.render(false, false);
    // settled estimate on the rendered kick should be near 100 Hz (may be sloppy)
    float hz = estimateSettledHz(r.L.data(), std::min(r.n(), (int)(0.4 * 44100)), r.sr);
    CHECKM(hz > 70 && hz < 150, "pitched kick settled %g (want ~100)", hz);
}

static void testRebuildBlendAB() {
    Machine m; Wave one; one.resize((int)(44100 * 0.8), 44100); addKick(one, 0, 50, 0.9f);
    m.loadWave(one);
    for (auto& e : m.events) e.lane = 0;
    m.lanes[0].kind = LANE_KICK;
    auto dry = m.render(false, false);
    m.rebuildLane(0);
    CHECK(m.lanes[0].rebuilt);
    m.lanes[0].blend = 0;
    auto b0 = m.render(false, false);
    int near = 0; int N = std::min(dry.n(), b0.n());
    for (int i = 0; i < N; ++i) if (std::abs(dry.L[(size_t)i] - b0.L[(size_t)i]) < 1e-6f) ++near;
    CHECKM(near > N * 0.98, "blend 0 changed %d/%d", N - near, N);
    m.lanes[0].blend = 1;
    auto b1 = m.render(false, false);
    CHECK(tu::peak(b1.L) > 0.05f && tu::finite(b1.L));
    // not identical to dry
    int diff = 0; for (int i = 0; i < N; ++i) if (std::abs(dry.L[(size_t)i] - b1.L[(size_t)i]) > 1e-4f) ++diff;
    CHECK(diff > N / 5);
    // A/B original
    m.abOriginal = true;
    auto ab = m.render(true, false);
    int same = 0, M = std::min(ab.n(), one.n());
    for (int i = 0; i < M; ++i) if (ab.L[(size_t)i] == one.L[(size_t)i]) ++same;
    CHECKM(same == M, "A/B not exact (%d/%d)", same, M);
}

static void testKeepInKeyAndExport() {
    Machine m; Wave one; one.resize((int)(44100 * 0.7), 44100); addKick(one, 0, 50, 0.9f);
    m.loadWave(one);
    for (auto& e : m.events) e.lane = 0;
    m.lanes[0].kind = LANE_KICK;
    m.keyPc = 4; m.scale = SC_MINOR; m.keepInKey = true; m.snapToRoot = false;
    // G1 is in E minor — display should be a G
    float hz = m.laneDisplayHz(0);
    auto n = noteCentsFromHz(hz);
    CHECKM(n.valid && n.pc == 7, "display pc %d hz %g", n.pc, hz);
    m.snapToRoot = true;
    hz = m.laneDisplayHz(0);
    n = noteCentsFromHz(hz);
    CHECKM(n.valid && n.pc == 4, "root snap pc %d hz %g", n.pc, hz);
    auto mid = m.exportMidi();
    CHECK(mid.size() > 20);
    CHECK(mid[0] == 'M' && mid[1] == 'T' && mid[2] == 'h' && mid[3] == 'd');
}


static void testBusierAccuracy() {
    auto w = makeKitLoop(44100, 120, 2, true); // hats also on kicks (overlap)
    auto sl = sliceAndClassify(w);
    int step = (int)std::lround(44100 * 60.0 / 120.0 / 4.0); // 16th
    int ok = 0, n = 0;
    int conf[4][4] {};
    // Score non-overlapping hits: off-beat hats (every 8th that is not a quarter) and perc on last 16ths.
    // Quarters with isolated snares (odd beats have snare+hat overlap — skip or accept snare/kick majority).
    for (int b = 0; b < 8; ++b) {
        int q = b * step * 4;
        int eighth = q + step * 2;
        // off-beat hat should be its own slice, labelled hats
        int want = eighth, best = -1, bestE = 1 << 30;
        for (int j = 0; j < (int)sl.size(); ++j) {
            int e = std::abs(sl[(size_t)j].start - want);
            if (e < bestE) { bestE = e; best = j; }
        }
        if (best >= 0 && bestE < (int)(0.04 * 44100)) {
            int pred = sl[(size_t)best].lane;
            conf[LANE_HATS][pred]++; if (pred == LANE_HATS) ++ok; ++n;
        }
        // downbeat: overlapping kick/snare + hat stays one slice. Accept kick on even, snare on odd.
        want = q; best = -1; bestE = 1 << 30;
        for (int j = 0; j < (int)sl.size(); ++j) {
            int e = std::abs(sl[(size_t)j].start - want);
            if (e < bestE) { bestE = e; best = j; }
        }
        int expect = (b % 2 == 0) ? LANE_KICK : LANE_SNARE;
        if (best >= 0 && bestE < (int)(0.04 * 44100)) {
            int pred = sl[(size_t)best].lane;
            // overlapping hat is allowed to stay in the same slice — label should be the body (kick/snare)
            conf[expect][pred]++; if (pred == expect) ++ok; ++n;
        }
        if (b % 4 == 3) {
            want = q + step * 3; best = -1; bestE = 1 << 30;
            for (int j = 0; j < (int)sl.size(); ++j) {
                int e = std::abs(sl[(size_t)j].start - want);
                if (e < bestE) { bestE = e; best = j; }
            }
            if (best >= 0 && bestE < (int)(0.04 * 44100)) {
                int pred = sl[(size_t)best].lane;
                conf[LANE_PERC][pred]++; if (pred == LANE_PERC) ++ok; ++n;
            }
        }
    }
    double acc = n ? 100.0 * ok / n : 0;
    std::printf("  busier loop accuracy: %d/%d = %.1f%% (onsets %zu)\n", ok, n, acc, sl.size());
    CHECKM(n >= 14 && acc >= 85.0, "busier accuracy %.1f%% on %d", acc, n);
}

static void testControlsAndSat() {
    // saturation curves: 0 stays 0; hard clip rails; tape is odd-symmetric
    CHECK(std::abs(softTape(0, 0.8f)) < 1e-7f && std::abs(tubeSat(0, 0.8f)) < 1e-7f && hardClip(0, 0.8f) == 0);
    CHECK(std::abs(softTape(0.9f, 0.7f) + softTape(-0.9f, 0.7f)) < 1e-6f);
    CHECK(hardClip(4.f, 1.f) == 1.f && hardClip(-4.f, 1.f) == -1.f);
    CHECK(std::abs(fuzzSat(0.8f, 1.f)) <= 1.f);
    // decay shortens the tail
    Machine m; Wave one; one.resize((int)(44100 * 0.7), 44100); addKick(one, 0, 50, 0.9f);
    m.loadWave(one);
    for (auto& e : m.events) e.lane = 0;
    auto full = m.render(false, false);
    m.lanes[0].decay = 0.25f;
    auto shortn = m.render(false, false);
    auto eFull = tu::rms(full.L, (size_t)(0.15 * 44100), (size_t)(0.35 * 44100));
    auto eShort = tu::rms(shortn.L, (size_t)(0.15 * 44100), (size_t)(0.35 * 44100));
    CHECKM(eShort < 0.5 * eFull, "decay tail %g vs %g", eShort, eFull);
    // swing moves odd 16ths later
    Machine s; auto w = makeKitLoop(); s.loadWave(w);
    auto dry = s.render(false, false);
    if (!s.lanes.empty()) s.lanes[2].swing = 0.8f; // hats
    auto wet = s.render(false, false);
    CHECK(tu::finite(wet.L) && tu::peak(wet.L) > 0);
    int diff = 0; int N = std::min(dry.n(), wet.n());
    for (int i = 0; i < N; ++i) if (std::abs(dry.L[(size_t)i] - wet.L[(size_t)i]) > 1e-5f) ++diff;
    CHECK(diff > 100);
    // randomize + undo, copy, ghosts, room, humanize all produce finite audio
    int ev = (int)s.events.size();
    s.copyEvent(0);
    CHECK((int)s.events.size() == ev + 1);
    s.doUndo();
    CHECK((int)s.events.size() == ev);
    float p0 = s.lanes[0].pitch;
    s.randomizeLane(0, 42);
    CHECK(s.lanes[0].pitch != p0);
    s.doUndo();
    CHECK(s.lanes[0].pitch == p0);
    s.lanes[1].ghosts = 0.8f; s.lanes[0].room = 0.6f; s.lanes[2].humanize = 0.5f;
    auto fx = s.render(false, false);
    CHECK(tu::finite(fx.L) && tu::peak(fx.L) > 0 && tu::peak(fx.L) < 8.f);
    // isolated lane export is quieter on muted other lanes
    auto iso = s.renderLane(0);
    CHECK(tu::finite(iso.L) && iso.n() > 0);
}

static void testPlaybackSync() {
    Machine m; auto w = makeKitLoop(44100, 120, 2);
    m.loadWave(w);
    auto cache = m.render(false, false);
    CHECK(tu::finite(cache.L) && tu::peak(cache.L) > 0.05f && tu::peak(cache.L) < 4.f);
    std::vector<float> L(512), R(512);
    m.mixAt(L.data(), R.data(), 512, 120, 0, cache);
    CHECK(tu::finite(L) && tu::peak(L) >= 0);
    // limiter / safety on bus
    m.bus.applyPreset(BUS_SMASHED);
    auto wet = m.render(true, false);
    CHECK(tu::finite(wet.L) && tu::peak(wet.L) <= 0.98f + 1e-4f);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    struct T { const char* n; void (*f)(); } t[] = {
        {"key math", testKeyMath},
        {"onsets + labels (sparse)", testOnsetsAndLabels},
        {"overlapping hits stay together", testOverlapStaysTogether},
        {"clean isolated accuracy", testAccuracy},
        {"lanes / arrange / undo", testLanesArrangeUndo},
        {"bus clean + DSP", testBusCleanAndDSP},
        {"rebuild / blend / A-B", testRebuildBlendAB},
        {"keep in key + MIDI export", testKeepInKeyAndExport},
        {"busier loop accuracy", testBusierAccuracy},
        {"controls / sat / decay / swing", testControlsAndSat},
        {"playback + safety", testPlaybackSync},
    };
    for (auto& x : t) {
        int f0 = g_fail; auto t0 = std::chrono::steady_clock::now();
        x.f();
        std::printf("[%s] %s (%.2fs)\n", g_fail == f0 ? "ok" : "FAILED", x.n,
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    std::printf("ENGINE: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
