#pragma once
#include "Dsp.h"

namespace cr {

enum Mode { MODE_WHOLE = 0, MODE_SPLIT = 1 };
enum BusPreset { BUS_CLEAN = 0, BUS_TAPE, BUS_BLOWN, BUS_SMASHED, BUS_BEDROOM, kBusPresets };
enum WholePreset { WP_CLEAN = 0, WP_TIGHT, WP_FAT, WP_LOFI, WP_WIDE, kWholePresets };
enum PartId { PART_KICK = 0, PART_SNARE, PART_HATS, PART_PERC, kParts };

inline const char* busPresetName(int p) {
    static const char* n[] = {"Clean", "Tape Warm", "Blown Speaker", "Smashed Room", "Bedroom Crunch"};
    return n[(p >= 0 && p < kBusPresets) ? p : 0];
}
inline const char* wholePresetName(int p) {
    static const char* n[] = {"Clean", "Tight Punch", "Fat Body", "Lo-Fi Crunch", "Wide Smash"};
    return n[(p >= 0 && p < kWholePresets) ? p : 0];
}
inline const char* partName(int p) {
    static const char* n[] = {"Kick", "Snare", "Hats", "Perc"};
    return n[(p >= 0 && p < kParts) ? p : 3];
}
inline const uint32_t kPartColour[kParts] = {
    0xFFE85A2A, // warm orange-red
    0xFF5B4FE8, // violet-blue
    0xFF2FA36B, // green
    0xFFE0B400  // golden yellow
};

struct PartParams {
    float level = 1, punch = 0, body = 0, decay = 0, crunch = 0, pan = 0, width = 1, sens = 0.5f;
    bool mute = false, listen = false;
};

struct Params {
    int mode = MODE_WHOLE;
    bool ab = false;
    float mix = 1, outputDb = 0, ceilingDb = -0.1f;
    bool lookahead = false;

    int wholePreset = WP_CLEAN;
    float wPunch = 0, wBody = 0, wSnap = 0, wCrunch = 0, wTone = 0, wGlue = 0, wRoom = 0, wWidth = 1;
    int wCrunchType = CR_TAPE;

    int bus = BUS_CLEAN;
    float bPunch = 0, bCrunch = 0, bSquash = 0, bPump = 0, bRoom = 0;
    int bCrunchType = CR_TAPE;

    float lowSplitHz = 170.f, highSplitHz = 5500.f;
    PartParams part[kParts];

    void applyBusPreset(int p) {
        bus = clampf((float)p, 0, kBusPresets - 1);
        p = bus;
        if (p == BUS_CLEAN) { bPunch = bCrunch = bSquash = bPump = bRoom = 0; bCrunchType = CR_TAPE; }
        else if (p == BUS_TAPE)    { bPunch = 0.25f; bCrunch = 0.32f; bCrunchType = CR_TAPE; bSquash = 0.22f; bPump = 0.08f; bRoom = 0.18f; }
        else if (p == BUS_BLOWN)   { bPunch = 0.35f; bCrunch = 0.62f; bCrunchType = CR_CLIP; bSquash = 0.28f; bPump = 0.10f; bRoom = 0.08f; }
        else if (p == BUS_SMASHED) { bPunch = 0.40f; bCrunch = 0.40f; bCrunchType = CR_TUBE; bSquash = 0.65f; bPump = 0.40f; bRoom = 0.42f; }
        else if (p == BUS_BEDROOM) { bPunch = 0.28f; bCrunch = 0.48f; bCrunchType = CR_TAPE; bSquash = 0.32f; bPump = 0.18f; bRoom = 0.26f; }
    }
    void applyWholePreset(int p) {
        wholePreset = clampf((float)p, 0, kWholePresets - 1);
        p = wholePreset;
        wCrunchType = CR_TAPE;
        if (p == WP_CLEAN) { wPunch = wBody = wSnap = wCrunch = wTone = wGlue = wRoom = 0; wWidth = 1; }
        else if (p == WP_TIGHT) { wPunch = 0.45f; wBody = -0.15f; wSnap = 0.40f; wCrunch = 0.08f; wTone = 0.10f; wGlue = 0.10f; wRoom = 0; wWidth = 1; }
        else if (p == WP_FAT)   { wPunch = 0.15f; wBody = 0.45f; wSnap = 0.10f; wCrunch = 0.18f; wCrunchType = CR_TAPE; wTone = -0.10f; wGlue = 0.28f; wRoom = 0.10f; wWidth = 1.15f; }
        else if (p == WP_LOFI)  { wPunch = 0.12f; wBody = 0.10f; wSnap = 0.05f; wCrunch = 0.48f; wCrunchType = CR_TAPE; wTone = -0.35f; wGlue = 0.15f; wRoom = 0.22f; wWidth = 0.85f; }
        else if (p == WP_WIDE)  { wPunch = 0.22f; wBody = 0.15f; wSnap = 0.18f; wCrunch = 0.12f; wTone = 0.05f; wGlue = 0.40f; wRoom = 0.18f; wWidth = 1.45f; }
    }
    bool busIsClean() const {
        return bPunch <= 1e-5f && bCrunch <= 1e-5f && bSquash <= 1e-5f && bPump <= 1e-5f && bRoom <= 1e-5f;
    }
    bool wholeIsBypass() const {
        return wPunch <= 1e-5f && std::fabs(wBody) <= 1e-5f && wSnap <= 1e-5f && wCrunch <= 1e-5f
            && std::fabs(wTone) <= 1e-5f && wGlue <= 1e-5f && wRoom <= 1e-5f && std::fabs(wWidth - 1.f) <= 1e-5f;
    }
    bool splitIsBypass() const {
        for (int i = 0; i < kParts; ++i) {
            const auto& p = part[i];
            if (p.mute || p.listen) return false;
            if (std::fabs(p.level - 1.f) > 1e-5f) return false;
            if (p.punch > 1e-5f || std::fabs(p.body) > 1e-5f || p.decay > 1e-5f || p.crunch > 1e-5f) return false;
            if (std::fabs(p.pan) > 1e-5f || std::fabs(p.width - 1.f) > 1e-5f) return false;
        }
        return true;
    }
    // Fully dry: bit-exact copy of the input (ceiling and lookahead do not run).
    bool isFullBypass() const {
        if (ab) return true;
        if (std::fabs(outputDb) > 1e-5f) return false;
        if (lookahead) return false;
        if (!busIsClean()) return false;
        if (mode == MODE_WHOLE) return wholeIsBypass();
        return splitIsBypass();
        // mix is irrelevant if wet == dry
    }
};

// One-pole tilt: mix of low-shelf and high-shelf, 0 = exact.
struct Tilt {
    Biquad lo, hi;
    void set(float amt, double sr) { // amt -1..1, darker..brighter
        lo.setLowShelf(400.f, -6.f * amt, sr);
        hi.setHighShelf(2500.f, 6.f * amt, sr);
    }
    float tick(float x) { return hi.tick(lo.tick(x)); }
    void reset() { lo.reset(); hi.reset(); }
};

struct TransientShaper {
    PeakFollower atk, sus;
    Agc agc;
    void prepare(double sr) {
        atk.setMs(0.4f, 40.f, sr);
        sus.setMs(20.f, 180.f, sr);
        agc.setMs(80.f, 250.f, sr);
    }
    void reset() { atk.reset(); sus.reset(); agc.reset(); }
    // punch >= 0 boosts attack; decay >= 0 shortens the tail after the hit.
    float gain(float x, float punch, float decay) {
        float a = atk.tick(x), s = sus.tick(x);
        float g = 1.f + punch * 1.6f * clampf(a - s, 0, 1);
        if (decay > 1e-5f) {
            float tail = clampf(s - a, 0, 1);
            g *= 1.f - decay * 0.85f * tail;
        }
        return g;
    }
    void tickStereo(float& l, float& r, float punch, float decay) {
        if (punch <= 1e-5f && decay <= 1e-5f) return;
        float mid = 0.5f * (l + r);
        float g = gain(mid, punch, decay);
        float yl = l * g, yr = r * g;
        float ag = agc.tick(mid, 0.5f * (yl + yr));
        l = yl * ag; r = yr * ag;
    }
};

struct Glue {
    PeakFollower fol;
    Agc agc;
    float thresh = 0.18f, ratio = 3.f;
    void prepare(double sr) {
        fol.setMs(12.f, 120.f, sr);
        agc.setMs(80.f, 250.f, sr);
    }
    void reset() { fol.reset(); agc.reset(); }
    void tickStereo(float& l, float& r, float amt) {
        if (amt <= 1e-5f) return;
        float mid = 0.5f * (l + r);
        float env = fol.tick(mid);
        float th = lerp(0.35f, 0.10f, amt);
        float rat = lerp(2.f, 6.f, amt);
        float g = 1.f;
        if (env > th) g = (th + (env - th) / rat) / (env + 1e-8f);
        float yl = lerp(l, l * g, amt);
        float yr = lerp(r, r * g, amt);
        float ag = agc.tick(mid, 0.5f * (yl + yr));
        l = yl * ag; r = yr * ag;
    }
};

struct Squash {
    PeakFollower fol;
    Agc agc;
    void prepare(double sr) {
        fol.setMs(1.5f, 80.f, sr);
        agc.setMs(60.f, 200.f, sr);
    }
    void reset() { fol.reset(); agc.reset(); }
    void tickStereo(float& l, float& r, float amt) {
        if (amt <= 1e-5f) return;
        float mid = 0.5f * (l + r);
        float env = fol.tick(mid);
        float th = 0.12f, rat = 8.f;
        float g = 1.f;
        if (env > th) g = (th + (env - th) / rat) / (env + 1e-8f);
        float wl = l * g, wr = r * g;
        float ag = agc.tick(mid, 0.5f * (wl + wr));
        wl *= ag; wr *= ag;
        l = lerp(l, wl, amt); r = lerp(r, wr, amt);
    }
};

struct Engine {
    Params p;
    double sr = 44100;
    int block = 512;

    // Whole / shared
    TransientShaper wholePunch, busPunch;
    Tilt wholeBodyL, wholeBodyR, wholeToneL, wholeToneR;
    Biquad snapL, snapR;
    Glue glue;
    CrunchOS wholeCrunch, busCrunch, partCrunch[kParts];
    Room wholeRoom, busRoom;
    Squash squash;
    RmsAgc busAgc;
    PeakFollower pumpFol;
    OnePole pumpLpL, pumpLpR;
    Ceiling ceil0;
    LookaheadLimiter la;
    bool laOn = false;
    int laSamples = 0;

    // Split
    LR4 lowLpL, lowLpR, lowHpL, lowHpR, highLpL, highLpR, highHpL, highHpR;
    float curLow = 0, curHigh = 0;
    TransientShaper partShaper[kParts];
    Tilt partToneL[kParts], partToneR[kParts];
    PeakFollower partAtk[kParts], partSus[kParts];

    // Meters (linear peak, decayed by the caller / processor)
    float inPeak = 0, outPeak = 0, ceilHold = 0;

    void prepare(double sampleRate, int maxBlock) {
        sr = sampleRate; block = std::max(16, maxBlock);
        auto prepShapers = [&]() {
            wholePunch.prepare(sr); busPunch.prepare(sr);
            glue.prepare(sr); squash.prepare(sr);
            busAgc.setMs(40.f, 90.f, sr);
            pumpFol.setMs(1.f, 180.f, sr);
            pumpLpL.setHz(150.f, sr); pumpLpR.setHz(150.f, sr);
            wholeRoom.prepare(sr); busRoom.prepare(sr);
            wholeCrunch.prepare(sr); busCrunch.prepare(sr);
            for (int i = 0; i < kParts; ++i) {
                partCrunch[i].prepare(sr);
                partShaper[i].prepare(sr);
                partAtk[i].setMs(0.4f, 40.f, sr);
                partSus[i].setMs(18.f, 160.f, sr);
            }
        };
        prepShapers();
        setSplits(p.lowSplitHz, p.highSplitHz, true);
        setWholeFilters();
        ceil0.setDb(p.ceilingDb);
        la.prepare(sr, 1.5f, p.ceilingDb);
        laSamples = la.latency();
        laOn = p.lookahead;
        reset();
    }

    void reset() {
        wholePunch.reset(); busPunch.reset(); glue.reset(); squash.reset();
        busAgc.reset();
        wholeBodyL.reset(); wholeBodyR.reset();
        wholeToneL.reset(); wholeToneR.reset();
        snapL.reset(); snapR.reset();
        wholeCrunch.reset(); busCrunch.reset();
        wholeRoom.reset(); busRoom.reset();
        pumpFol.reset(); pumpLpL.reset(); pumpLpR.reset();
        la.reset();
        lowLpL.reset(); lowLpR.reset(); lowHpL.reset(); lowHpR.reset();
        highLpL.reset(); highLpR.reset(); highHpL.reset(); highHpR.reset();
        for (int i = 0; i < kParts; ++i) {
            partCrunch[i].reset(); partShaper[i].reset();
            partToneL[i].reset(); partToneR[i].reset();
            partAtk[i].reset(); partSus[i].reset();
        }
        inPeak = outPeak = ceilHold = 0;
    }

    int latencySamples() const { return (p.lookahead || laOn) ? laSamples : 0; }

    void setParams(const Params& np) {
        bool splitsChanged = (std::fabs(np.lowSplitHz - p.lowSplitHz) > 0.5f)
                          || (std::fabs(np.highSplitHz - p.highSplitHz) > 0.5f);
        bool wholeEqChanged = (np.wBody != p.wBody) || (np.wTone != p.wTone) || (np.wSnap != p.wSnap);
        bool ceilChanged = (np.ceilingDb != p.ceilingDb) || (np.lookahead != p.lookahead);
        p = np;
        if (splitsChanged) setSplits(p.lowSplitHz, p.highSplitHz, false);
        if (wholeEqChanged) setWholeFilters();
        if (ceilChanged) {
            ceil0.setDb(p.ceilingDb);
            // Recreate lookahead with same delay length (sr unchanged).
            int keep = laSamples;
            la.prepare(sr, 1.5f, p.ceilingDb);
            laSamples = la.latency();
            (void)keep;
        }
        laOn = p.lookahead;
        for (int i = 0; i < kParts; ++i) {
            partToneL[i].set(p.part[i].body, sr);
            partToneR[i].set(p.part[i].body, sr);
        }
    }

    void setSplits(float lo, float hi, bool force) {
        lo = clampf(lo, 60.f, 400.f);
        hi = clampf(hi, 2000.f, 12000.f);
        if (hi < lo + 200.f) hi = lo + 200.f;
        if (!force && std::fabs(lo - curLow) < 0.5f && std::fabs(hi - curHigh) < 0.5f) return;
        curLow = lo; curHigh = hi;
        lowLpL.setLP(lo, sr); lowLpR.setLP(lo, sr);
        lowHpL.setHP(lo, sr); lowHpR.setHP(lo, sr);
        highLpL.setLP(hi, sr); highLpR.setLP(hi, sr);
        highHpL.setHP(hi, sr); highHpR.setHP(hi, sr);
    }
    void setWholeFilters() {
        wholeBodyL.set(p.wBody, sr); wholeBodyR.set(p.wBody, sr);
        wholeToneL.set(p.wTone, sr); wholeToneR.set(p.wTone, sr);
        snapL.setPeak(3500.f, 8.f * p.wSnap, 0.9f, sr);
        snapR.setPeak(3500.f, 8.f * p.wSnap, 0.9f, sr);
    }

    void process(float* L, float* R, int n) {
        if (n <= 0) return;
        float inPk = 0, outPk = 0, hold = ceilHold * 0.95f;
        const float outG = dbToGain(p.outputDb);
        const float ceilLin = dbToGain(p.ceilingDb);
        const bool bypass = p.isFullBypass();
        const bool useLA = p.lookahead;

        if (bypass && !useLA) {
            for (int i = 0; i < n; ++i) {
                inPk = std::max(inPk, std::max(std::fabs(L[i]), std::fabs(R[i])));
                outPk = inPk;
            }
            inPeak = inPk; outPeak = outPk; ceilHold = 0;
            return; // bit-exact: buffer left untouched
        }

        for (int i = 0; i < n; ++i) {
            float inL = L[i], inR = R[i];
            inPk = std::max(inPk, std::max(std::fabs(inL), std::fabs(inR)));

            float dL = inL, dR = inR;
            float wL = inL, wR = inR;

            if (!p.ab) {
                if (p.mode == MODE_SPLIT) processSplit(wL, wR);
                else processWhole(wL, wR);

                // Mix (wet against the dry input)
                if (p.mix < 1.f - 1e-7f) {
                    wL = lerp(dL, wL, p.mix);
                    wR = lerp(dR, wR, p.mix);
                }

                processBus(wL, wR);
                if (std::fabs(outG - 1.f) > 1e-7f) { wL *= outG; wR *= outG; }
            } else {
                // A/B dry: still pass through the same delay as the wet path if lookahead is on.
                wL = dL; wR = dR;
            }

            float oL, oR;
            if (useLA) {
                la.tick(wL, wR, oL, oR);
            } else if (p.ab || p.isFullBypass()) {
                oL = wL; oR = wR;
            } else {
                oL = ceil0.tick(wL);
                oR = ceil0.tick(wR);
            }
            L[i] = oL; R[i] = oR;
            float pk = std::max(std::fabs(oL), std::fabs(oR));
            outPk = std::max(outPk, pk);
            if (pk >= ceilLin * 0.995f) hold = 1.f;
        }
        inPeak = inPk; outPeak = outPk; ceilHold = hold;
    }

    void processWhole(float& l, float& r) {
        if (p.wholeIsBypass()) return;
        wholePunch.tickStereo(l, r, p.wPunch, 0);
        if (std::fabs(p.wBody) > 1e-5f) { l = wholeBodyL.tick(l); r = wholeBodyR.tick(r); }
        if (p.wSnap > 1e-5f) { l = snapL.tick(l); r = snapR.tick(r); }
        if (std::fabs(p.wTone) > 1e-5f) { l = wholeToneL.tick(l); r = wholeToneR.tick(r); }
        if (p.wCrunch > 1e-5f) {
            float ol, or_;
            wholeCrunch.tick(l, r, p.wCrunchType, p.wCrunch, ol, or_);
            l = ol; r = or_;
        }
        glue.tickStereo(l, r, p.wGlue);
        if (p.wRoom > 1e-5f) { float ol, or_; wholeRoom.tick(l, r, p.wRoom, ol, or_); l = ol; r = or_; }
        applyWidth(l, r, p.wWidth);
    }

    void processSplit(float& l, float& r) {
        // Complementary 3-way: mid = in - low - high, then mid is split into
        // snare (transient) + perc (the rest). Sum of the four parts is exactly `in`.
        float inL = l, inR = r;
        // Cascaded LR4: low / mid / high. Mute/listen isolate bands musically.
        // (Unity at default is a bit-exact bypass in process(), so we do not
        // need the bands to sum to identity here.)
        float kL = lowLpL.tick(inL), kR = lowLpR.tick(inR);
        float restL = lowHpL.tick(inL), restR = lowHpR.tick(inR);
        float mL = highLpL.tick(restL), mR = highLpR.tick(restR);
        float hL = highHpL.tick(restL), hR = highHpR.tick(restR);

        // Transient weight on the mid band (snare vs perc)
        float midMono = 0.5f * (mL + mR);
        float atk = partAtk[PART_SNARE].tick(midMono);
        float sus = partSus[PART_SNARE].tick(midMono);
        float sens = clampf(p.part[PART_SNARE].sens, 0.f, 1.f);
        float rise = atk - sus;
        float tw = clampf(rise / (atk + 1e-6f), 0.f, 1.f);
        // Sensitivity: lower = less of the mid claimed by snare (more stays in perc)
        float snareW = tw * (0.25f + 0.75f * sens);
        float sL = mL * snareW, sR = mR * snareW;
        float pL = mL - sL, pR = mR - sR;

        float bL[kParts] = {kL, sL, hL, pL};
        float bR[kParts] = {kR, sR, hR, pR};

        bool anyListen = false;
        for (int i = 0; i < kParts; ++i) if (p.part[i].listen) anyListen = true;

        float oL = 0, oR = 0;
        for (int i = 0; i < kParts; ++i) {
            const auto& pp = p.part[i];
            if (pp.mute) continue;
            if (anyListen && !pp.listen) continue;
            float xL = bL[i], xR = bR[i];
            partShaper[i].tickStereo(xL, xR, pp.punch, pp.decay);
            if (std::fabs(pp.body) > 1e-5f) { xL = partToneL[i].tick(xL); xR = partToneR[i].tick(xR); }
            if (pp.crunch > 1e-5f) {
                float ol, or_;
                partCrunch[i].tick(xL, xR, p.wCrunchType, pp.crunch, ol, or_);
                xL = ol; xR = or_;
            }
            if (std::fabs(pp.level - 1.f) > 1e-7f) { xL *= pp.level; xR *= pp.level; }
            applyPan(xL, xR, pp.pan);
            applyWidth(xL, xR, pp.width);
            oL += xL; oR += xR;
        }
        l = oL; r = oR;
    }

    void processBus(float& l, float& r) {
        if (p.busIsClean()) return;
        float dL = l, dR = r;
        if (p.bPunch > 1e-5f) busPunch.tickStereo(l, r, p.bPunch, 0);
        if (p.bCrunch > 1e-5f) {
            float ol, or_;
            busCrunch.tick(l, r, p.bCrunchType, p.bCrunch, ol, or_);
            l = ol; r = or_;
        }
        if (p.bSquash > 1e-5f) squash.tickStereo(l, r, p.bSquash);
        if (p.bPump > 1e-5f) {
            float low = 0.5f * (pumpLpL.tick(l) + pumpLpR.tick(r));
            float env = pumpFol.tick(low);
            float g = 1.f - p.bPump * 0.75f * clampf(env * 3.f, 0, 1);
            l *= g; r *= g;
        }
        if (p.bRoom > 1e-5f) { float ol, or_; busRoom.tick(l, r, p.bRoom, ol, or_); l = ol; r = or_; }
        // Match the whole bus stage to the dry bus input so a preset is a colour, not a level jump.
        float ag = busAgc.tick(0.5f * (dL + dR), 0.5f * (l + r));
        l *= ag; r *= ag;
    }
};

} // namespace cr
