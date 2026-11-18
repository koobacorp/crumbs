#include "TestUtil.h"
#include "Engine.h"
#include <chrono>
#include <cmath>
using namespace cr;

static void addKick(std::vector<float>& L, std::vector<float>& R, int at, double sr, float hz = 50, float amp = 0.9f) {
    int n = (int)L.size(); float ph = 0;
    for (int i = 0; i < (int)(0.4 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        float f = hz * std::exp2(18.f * std::exp(-t / 0.008f) / 12.f);
        float env = std::exp(-t / 0.28f);
        ph += 2.f * float(M_PI) * f / float(sr);
        float s = amp * env * std::sin(ph);
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s;
    }
}
static void addSnare(std::vector<float>& L, std::vector<float>& R, int at, double sr, float amp = 0.7f) {
    int n = (int)L.size(); uint32_t rng = 0x12345678u + (uint32_t)at;
    for (int i = 0; i < (int)(0.22 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        rng = rng * 1664525u + 1013904223u;
        float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
        float body = 0.45f * std::sin(2 * float(M_PI) * 200.f * t) * std::exp(-t / 0.05f);
        float s = amp * (body + 0.7f * noise * std::exp(-t / 0.09f) + 0.5f * noise * std::exp(-t / 0.008f));
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s * 0.95f;
    }
}
static void addHat(std::vector<float>& L, std::vector<float>& R, int at, double sr, float amp = 0.35f, float dec = 0.05f) {
    int n = (int)L.size(); uint32_t rng = 0xC0FFEEu + (uint32_t)at * 17;
    float hp = 0, a = 1.f - std::exp(-2.f * float(M_PI) * 7000.f / float(sr));
    for (int i = 0; i < (int)(0.2 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        rng = rng * 1664525u + 1013904223u;
        float noise = (float((rng >> 9) & 0xFFFF) / 32768.f - 1.f);
        hp += a * (noise - hp);
        float s = amp * (noise - hp) * std::exp(-t / dec);
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s;
    }
}
static void addPerc(std::vector<float>& L, std::vector<float>& R, int at, double sr, float hz = 220, float amp = 0.5f) {
    int n = (int)L.size(); float ph = 0;
    for (int i = 0; i < (int)(0.15 * sr) && at + i < n; ++i) {
        float t = float(i) / float(sr);
        ph += 2.f * float(M_PI) * hz / float(sr);
        float s = amp * std::sin(ph) * std::exp(-t / 0.08f);
        L[(size_t)(at + i)] += s; R[(size_t)(at + i)] += s;
    }
}

struct Buf {
    std::vector<float> L, R;
    Buf(int n = 0) { L.assign((size_t)n, 0); R.assign((size_t)n, 0); }
    int n() const { return (int)L.size(); }
    void process(Engine& e, int block = 256) {
        int i = 0;
        while (i < n()) {
            int m = std::min(block, n() - i);
            e.process(L.data() + i, R.data() + i, m);
            i += m;
        }
    }
};

static Buf makeKit(double sr = 44100, double bpm = 120, int bars = 2, float kickAmp = 0.9f) {
    int beats = bars * 4;
    int n = (int)std::lround(sr * 60.0 * beats / bpm);
    Buf b(n);
    int step = (int)std::lround(sr * 60.0 / bpm / 4.0);
    for (int q = 0; q < beats; ++q) {
        int at = q * step * 4;
        if (q % 2 == 0) addKick(b.L, b.R, at, sr, 50, kickAmp);
        else addSnare(b.L, b.R, at, sr, 0.7f);
        addHat(b.L, b.R, at, sr, 0.35f, 0.04f);
        addHat(b.L, b.R, at + step * 2, sr, 0.28f, 0.04f);
        if (q % 4 == 3) addPerc(b.L, b.R, at + step * 3, sr, 330, 0.45f);
    }
    return b;
}

static void testSatAndClip() {
    CHECK(std::abs(softTape(0, 0.8f)) < 1e-7f && std::abs(tubeSat(0, 0.8f)) < 1e-7f && hardClip(0, 0.8f) == 0);
    CHECK(std::abs(softTape(0.9f, 0.7f) + softTape(-0.9f, 0.7f)) < 1e-6f);
    CHECK(hardClip(4.f, 1.f) == 1.f && hardClip(-4.f, 1.f) == -1.f);
    CHECK(std::abs(fuzzSat(0.8f, 1.f)) <= 1.f);
    CHECK(crunchSample(0.3f, CR_TAPE, 0.f) == 0.3f);
    float c = dbToGain(-0.1f);
    CHECK(clipToCeil(0.5f, c) == 0.5f);
    CHECK(clipToCeil(0.f, c) == 0.f);
    CHECK(std::abs(clipToCeil(-0.4f, c) + 0.4f) < 1e-12f);
    CHECK(clipToCeil(4.f, c) <= c + 1e-6f);
    CHECK(clipToCeil(-4.f, c) >= -c - 1e-6f);
    // pan 0 / width 1 leave a stereo pair untouched
    float l = 0.31f, r = -0.17f;
    applyPan(l, r, 0.f); applyWidth(l, r, 1.f);
    CHECK(l == 0.31f && r == -0.17f);
}

static void testCleanBitExact() {
    for (double sr : {44100.0, 48000.0}) {
        Engine e; e.prepare(sr, 256);
        CHECK(e.latencySamples() == 0);
        auto b = makeKit(sr, 120, 2, 0.95f);
        // also a 0 dBFS burst
        if (b.n() > 10) { b.L[8] = 1.f; b.R[8] = -1.f; }
        auto dryL = b.L, dryR = b.R;
        b.process(e, 256);
        int n = b.n();
        int same = tu::ident(b.L, dryL) + tu::ident(b.R, dryR);
        CHECKM(same == n * 2, "clean bit-exact %d/%d at sr=%.0f peak was %g", same, n * 2, sr, tu::peak(dryL));
        CHECK(e.latencySamples() == 0);
    }
}

static void testSplitNull() {
    Engine e; e.prepare(44100, 512);
    Params p; p.mode = MODE_SPLIT;
    e.setParams(p);
    auto b = makeKit(44100, 120, 2, 0.9f);
    auto dryL = b.L, dryR = b.R;
    b.process(e, 128);
    CHECK(tu::ident(b.L, dryL) == (int)b.L.size());
    CHECK(tu::ident(b.R, dryR) == (int)b.R.size());
    // moved crossovers, still unity
    p.lowSplitHz = 120; p.highSplitHz = 7000;
    e.prepare(44100, 512); e.setParams(p);
    Buf c = makeKit();
    auto dL = c.L, dR = c.R;
    c.process(e, 64);
    double rd = tu::rmsDiff(c.L, dL);
    CHECKM(rd < 1e-7, "split reconstruct rms %g", rd);
}

static void testLatencyImpulse() {
    // Default: peak of an impulse stays at sample 0, reported 0.
    for (double sr : {44100.0, 48000.0}) {
        Engine e; e.prepare(sr, 64);
        Params p; // defaults, plus some processing so we are not the early-return path
        p.wPunch = 0.3f; p.wCrunch = 0.4f; p.wGlue = 0.2f;
        e.setParams(p);
        CHECK(e.latencySamples() == 0);
        Buf b((int)sr / 10);
        b.L[0] = 1; b.R[0] = 1;
        b.process(e, 64);
        int k = tu::argmaxAbs(b.L);
        CHECKM(k == 0, "impulse peak at %d (sr=%.0f) with processing on, reported %d", k, sr, e.latencySamples());
        CHECK(tu::finite(b.L) && tu::peak(b.L) <= dbToGain(p.ceilingDb) + 1e-5f);
    }
    // Lookahead: peak delayed by exactly latencySamples()
    Engine e; e.prepare(48000, 64);
    Params p; p.lookahead = true; p.wPunch = 0.2f;
    e.setParams(p);
    int lat = e.latencySamples();
    CHECKM(lat > 0 && lat == (int)std::lround(0.0015 * 48000), "lookahead samples %d", lat);
    Buf b(4000);
    b.L[0] = 1; b.R[0] = 0.5f;
    b.process(e, 64);
    int k = tu::argmaxAbs(b.L);
    CHECKM(k == lat, "lookahead impulse at %d want %d", k, lat);
    CHECK(tu::peak(b.L) <= dbToGain(p.ceilingDb) + 1e-5f);
}

static void testCeilingStress() {
    const float ceil = dbToGain(-0.1f);
    auto run = [&](const char* name, Params p, double sr, float boost) {
        Engine e; e.prepare(sr, 512);
        e.setParams(p);
        auto b = makeKit(sr, 120, 2, 0.95f);
        for (auto& v : b.L) v *= boost;
        for (auto& v : b.R) v *= boost;
        // plus a square-like click
        if (b.n() > 1000) { for (int i = 200; i < 240; ++i) { b.L[(size_t)i] = boost; b.R[(size_t)i] = -boost; } }
        b.process(e, 512);
        float pk = tu::peak2(b.L, b.R);
        CHECKM(tu::finite(b.L) && tu::finite(b.R), "%s not finite", name);
        bool bypass = p.isFullBypass() && !p.lookahead;
        if (!bypass)
            CHECKM(pk <= ceil + 1e-5f, "%s peak %g > ceil %g (sr=%.0f boost=%g)", name, pk, ceil, sr, boost);
        std::printf("    %-32s  peak=%.5f  ceil=%.5f  sr=%.0f  boost=%.1f  bypass=%d\n", name, pk, ceil, sr, boost, (int)bypass);
        return pk;
    };
    // For bypass we must NOT enforce the ceiling. Separate check:
    {
        Engine e; e.prepare(44100, 256);
        Buf b(2048); b.L[10] = 1; b.R[10] = 1;
        b.process(e);
        CHECK(b.L[10] == 1.f); // clean does not clip legal? 1.0 is above -0.1dB but Clean bypasses ceiling
    }
    Params sm; sm.applyBusPreset(BUS_SMASHED); sm.bPunch = 1; sm.bCrunch = 1; sm.bSquash = 1; sm.bPump = 1; sm.bRoom = 1;
    run("Smashed Room maxed 44.1", sm, 44100, 1.0f);
    run("Smashed Room maxed 48k +6 dB", sm, 48000, 2.0f);
    Params wh; wh.wPunch = 1; wh.wBody = 1; wh.wSnap = 1; wh.wCrunch = 1; wh.wGlue = 1; wh.wRoom = 1; wh.wWidth = 2;
    wh.applyBusPreset(BUS_SMASHED);
    run("Whole max + Smashed 48k +6 dB", wh, 48000, 2.0f);
    Params sp; sp.mode = MODE_SPLIT;
    for (int i = 0; i < kParts; ++i) { sp.part[i].level = 2; sp.part[i].punch = 1; sp.part[i].crunch = 1; sp.part[i].decay = 0.5f; }
    sp.applyBusPreset(BUS_SMASHED);
    run("Split max + Smashed 44.1 +6 dB", sp, 44100, 2.0f);
    Params la = wh; la.lookahead = true;
    run("Lookahead + Whole max 48k +6 dB", la, 48000, 2.0f);

    // several block sizes
    for (int bs : {64, 256, 512, 1024}) {
        Engine e; e.prepare(48000, bs);
        Params p; p.applyBusPreset(BUS_SMASHED); p.bPunch = 1; p.bCrunch = 1; p.bSquash = 1;
        e.setParams(p);
        auto b = makeKit(48000, 120, 1, 1.0f);
        for (auto& v : b.L) v *= 2; for (auto& v : b.R) v *= 2;
        b.process(e, bs);
        CHECKM(tu::peak2(b.L, b.R) <= ceil + 1e-5f, "block %d peak %g", bs, tu::peak2(b.L, b.R));
    }
}

static void testBusPresetsAndAgc() {
    Engine e; e.prepare(44100, 256);
    auto kit = makeKit();
    auto run = [&](Params p) {
        Engine x; x.prepare(44100, 256); x.setParams(p);
        Buf b = kit;
        b.process(x);
        return tu::rms(b.L);
    };
    Params c;
    double rClean = run(c);
    for (int i = 1; i < kBusPresets; ++i) {
        Params p; p.applyBusPreset(i);
        Engine x; x.prepare(44100, 256); x.setParams(p);
        Buf b = kit; auto dry = b.L;
        b.process(x);
        CHECK(tu::finite(b.L) && tu::peak(b.L) > 0.02f);
        CHECK(tu::peak(b.L) <= dbToGain(-0.1f) + 1e-5f);
        // clearly different from clean
        CHECK(tu::rmsDiff(b.L, dry) > 1e-4);
        // A preset is a colour, not a level jump: within about ±1 dB RMS of Clean.
        double r = tu::rms(b.L);
        double db = 20 * std::log10(std::max(r, 1e-8) / std::max(rClean, 1e-8));
        CHECKM(std::fabs(db) <= 1.1, "preset %s RMS %+0.2f dB vs clean (want ±1 dB)", busPresetName(i), db);
        std::printf("    bus %-16s  rms %+0.2f dB vs clean\n", busPresetName(i), db);
        // back to clean is exact
        p.applyBusPreset(BUS_CLEAN);
        x.setParams(p); x.reset();
        Buf c2 = kit; auto d2 = c2.L;
        c2.process(x);
        CHECK(tu::ident(c2.L, d2) == c2.n());
    }
    // Whole presets also produce audio and respect the ceiling
    for (int i = 1; i < kWholePresets; ++i) {
        Params p; p.applyWholePreset(i);
        Engine x; x.prepare(44100, 256); x.setParams(p);
        Buf b = kit; b.process(x);
        CHECK(tu::finite(b.L) && tu::peak(b.L) <= dbToGain(-0.1f) + 1e-5f && tu::peak(b.L) > 0.02f);
    }
}

static void testSplitSanity() {
    // Isolated hits: Listen Kick during a kick should be louder than Listen Hats, and vice versa.
    const double sr = 44100;
    Buf b((int)sr * 2);
    int kAt = 0, sAt = (int)(0.5 * sr), hAt = (int)(1.0 * sr), pAt = (int)(1.5 * sr);
    addKick(b.L, b.R, kAt, sr, 50, 0.9f);
    addSnare(b.L, b.R, sAt, sr, 0.7f);
    addHat(b.L, b.R, hAt, sr, 0.5f, 0.04f);
    addPerc(b.L, b.R, pAt, sr, 220, 0.6f);

    auto listenRms = [&](int part, int at, int win) {
        Engine e; e.prepare(sr, 256);
        Params p; p.mode = MODE_SPLIT;
        for (int i = 0; i < kParts; ++i) p.part[i].listen = (i == part);
        e.setParams(p);
        Buf x = b;
        x.process(e);
        return tu::rms(x.L, (size_t)at, (size_t)(at + win));
    };
    int win = (int)(0.12 * sr);
    double kOnK = listenRms(PART_KICK, kAt, win), hOnK = listenRms(PART_HATS, kAt, win);
    double hOnH = listenRms(PART_HATS, hAt, win), kOnH = listenRms(PART_KICK, hAt, win);
    double sOnS = listenRms(PART_SNARE, sAt, win);
    double pOnP = listenRms(PART_PERC, pAt, win);
    std::printf("    listen  kick@kick=%.4f hats@kick=%.4f  hats@hat=%.4f kick@hat=%.4f  snare@snare=%.4f perc@perc=%.4f\n",
                kOnK, hOnK, hOnH, kOnH, sOnS, pOnP);
    CHECKM(kOnK > hOnK * 2.0, "kick listen on kick %g vs hats %g", kOnK, hOnK);
    CHECKM(hOnH > kOnH * 2.0, "hat listen on hat %g vs kick %g", hOnH, kOnH);
    CHECKM(sOnS > 0.02, "snare listen energy %g", sOnS);
    CHECKM(pOnP > 0.01, "perc listen energy %g", pOnP);
    // Mute hats should drop hat-window energy vs full
    Engine e; e.prepare(sr, 256);
    Params p; p.mode = MODE_SPLIT; p.part[PART_HATS].mute = true;
    e.setParams(p);
    Buf x = b; x.process(e);
    double mutedHats = tu::rms(x.L, (size_t)hAt, (size_t)(hAt + win));
    Engine e2; e2.prepare(sr, 256);
    Params p2; p2.mode = MODE_SPLIT; e2.setParams(p2);
    Buf y = b; y.process(e2);
    double fullHats = tu::rms(y.L, (size_t)hAt, (size_t)(hAt + win));
    CHECKM(mutedHats < 0.4 * fullHats, "mute hats %g vs %g", mutedHats, fullHats);
}

static void testControlsMove() {
    Engine e; e.prepare(44100, 256);
    auto kit = makeKit();
    auto dry = kit;
    // Each meaningful control should change the audio (except ones at bypass).
    struct C { const char* n; Params p; };
    std::vector<C> cs;
    auto add = [&](const char* n, Params p) { cs.push_back({n, p}); };
    { Params p; p.wPunch = 0.8f; add("punch", p); }
    { Params p; p.wBody = 0.8f; add("body", p); }
    { Params p; p.wSnap = 0.8f; add("snap", p); }
    { Params p; p.wCrunch = 0.7f; add("crunch", p); }
    { Params p; p.wTone = -0.7f; add("tone", p); }
    { Params p; p.wGlue = 0.7f; add("glue", p); }
    { Params p; p.wRoom = 0.6f; add("room", p); }
    { Params p; p.wWidth = 0.2f; add("width", p); }
    { Params p; p.outputDb = -6.f; add("output", p); }
    { Params p; p.mix = 0.5f; p.wPunch = 0.9f; add("mix", p); }
    { Params p; p.applyBusPreset(BUS_TAPE); add("tape", p); }
    { Params p; p.mode = MODE_SPLIT; p.part[0].level = 0.2f; add("kick level", p); }
    int ok = 0;
    for (auto& c : cs) {
        Engine x; x.prepare(44100, 256); x.setParams(c.p);
        Buf b = kit; b.process(x);
        CHECKM(tu::finite(b.L) && tu::finite(b.R), "%s not finite", c.n);
        CHECKM(tu::peak2(b.L, b.R) <= dbToGain(-0.1f) + 1e-5f || c.p.isFullBypass(), "%s peaked %g", c.n, tu::peak(b.L));
        bool changed = tu::rmsDiff(b.L, dry.L) > 1e-5;
        CHECKM(changed, "%s did not change the signal", c.n);
        if (changed) ++ok;
    }
    std::printf("    controls moved audio: %d/%d\n", ok, (int)cs.size());
    // Mix at 0 with processing equals dry (and under ceiling... dry may be >ceil, mix 0 is dry)
    {
        Params p; p.wPunch = 1; p.mix = 0;
        Engine x; x.prepare(44100, 256); x.setParams(p);
        Buf b = kit; auto d = b.L;
        b.process(x);
        // mix 0 still goes through ceiling because processing path is "on"
        // Plan: Mix at 0% matches dry (bus off). If ceiling is on the wet+dry mix, mix 0 is dry then ceiling.
        // I'll accept that mix 0 + ceiling may soften overs. On this kit (peak ~1.27?) check either exact or only ceiling difference.
        float pk = tu::peak(d);
        if (pk <= dbToGain(-0.1f)) {
            CHECKM(tu::ident(b.L, d) == b.n(), "mix 0 not exact on legal dry");
        } else {
            CHECK(tu::peak(b.L) <= dbToGain(-0.1f) + 1e-5f);
        }
    }
    // Output -inf ~ silence
    {
        Params p; p.outputDb = -120.f; p.wPunch = 0.5f;
        Engine x; x.prepare(44100, 256); x.setParams(p);
        Buf b = kit; b.process(x);
        CHECK(tu::peak(b.L) < 1e-5f);
    }
}

static void testCpu() {
    Engine e; e.prepare(48000, 512);
    Params p; p.mode = MODE_SPLIT;
    p.applyBusPreset(BUS_SMASHED);
    p.wCrunch = 0.5f;
    for (int i = 0; i < kParts; ++i) { p.part[i].punch = 0.5f; p.part[i].crunch = 0.4f; }
    e.setParams(p);
    const int sr = 48000, sec = 2, n = sr * sec, bs = 512;
    std::vector<float> L((size_t)n), R((size_t)n);
    for (int i = 0; i < n; ++i) { L[(size_t)i] = 0.2f * std::sin(2 * float(M_PI) * 80 * i / sr); R[(size_t)i] = L[(size_t)i]; }
    // warm up
    e.process(L.data(), R.data(), bs);
    auto t0 = std::chrono::steady_clock::now();
    int blocks = 0;
    for (int i = 0; i < n; i += bs) {
        int m = std::min(bs, n - i);
        e.process(L.data() + i, R.data() + i, m);
        ++blocks;
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double per = ms / blocks;
    double rt = 1000.0 * bs / sr;
    std::printf("    CPU: %.3f ms / 512-sample block (real-time is %.3f ms), 2s in %.2f ms\n", per, rt, ms);
    CHECKM(per < 1.0, "CPU %.3f ms/block >= 1 ms", per);
    CHECKM(ms < 2000.0, "2s buffer took %.2f ms (not under real time?)", ms);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    struct T { const char* n; void (*f)(); } t[] = {
        {"sat / ceiling / pan-width identity", testSatAndClip},
        {"Clean bit-exact bypass (44.1 and 48k)", testCleanBitExact},
        {"Split unity reconstruction", testSplitNull},
        {"latency vs impulse (0 and lookahead)", testLatencyImpulse},
        {"ceiling holds on stress signals", testCeilingStress},
        {"bus + whole presets, AGC, return to Clean", testBusPresetsAndAgc},
        {"Split listen/mute sanity on a kit", testSplitSanity},
        {"controls change audio, mix 0, output -inf", testControlsMove},
        {"CPU budget", testCpu},
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
