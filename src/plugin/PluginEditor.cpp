#include "PluginEditor.h"
using namespace cr;

static juce::Colour beige() { return juce::Colour(0xffe6d4b5); }
static juce::Colour ink()   { return juce::Colour(0xff2a2420); }
static juce::Colour muted() { return juce::Colour(0xff7a7268); }
static juce::Colour wash()  { return juce::Colour(0xffd4c3a4); }
static juce::Colour line()  { return juce::Colour(0xffc4b399); }
static juce::Colour partCol(int i) { return juce::Colour(kPartColour[juce::jlimit(0, kParts - 1, i)]); }

static void drawOrb(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float alpha, bool selected) {
    auto cx = r.getCentre();
    float rad = juce::jmin(r.getWidth(), r.getHeight()) * (selected ? 0.38f : 0.34f);
    juce::ColourGradient gr(c.withAlpha(alpha), cx.x, cx.y, c.withAlpha(0.0f), cx.x + rad, cx.y, true);
    g.setGradientFill(gr);
    g.fillEllipse(cx.x - rad, cx.y - rad, rad * 2, rad * 2);
    g.setColour(c.withAlpha(alpha * 0.20f));
    g.fillEllipse(cx.x - rad * 1.4f, cx.y - rad * 1.4f, rad * 2.8f, rad * 2.8f);
    if (selected) {
        g.setColour(ink().withAlpha(0.85f));
        g.drawEllipse(cx.x - rad * 1.12f, cx.y - rad * 1.12f, rad * 2.24f, rad * 2.24f, 1.6f);
    }
}

static void tiny(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c,
                 juce::Justification j = juce::Justification::centred) {
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(10.0f)).withExtraKerningFactor(0.10f));
    g.drawText(t.toUpperCase(), r, j, false);
}
static void valueFont(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c) {
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText(t, r, juce::Justification::centred, false);
}
static void chip(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on) {
    g.setColour(on ? ink() : wash());
    g.fillRoundedRectangle(r, 4.f);
    g.setColour(on ? beige() : ink());
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(t, r, juce::Justification::centred, false);
}
static void knob(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, const juce::String& val, juce::Colour accent) {
    // Compact: value then label, so a tall cell does not float the caption mid-window.
    auto box = r.withHeight(juce::jmin(r.getHeight(), 88.f));
    auto v = box.removeFromTop(36);
    auto lab = box.removeFromTop(16);
    valueFont(g, v, val, accent);
    tiny(g, lab, label, muted());
}
static juce::String fmt01(float x) { return juce::String(x, 2); }
static juce::String fmtSigned(float x) {
    if (std::abs(x) < 0.005f) return "0.00";
    return (x > 0 ? "+" : "") + juce::String(x, 2);
}
static juce::String fmtHz(float x) {
    if (x >= 1000.f) return juce::String(x / 1000.f, 1) + " kHz";
    return juce::String((int)std::lround(x)) + " Hz";
}
static juce::String fmtDb(float x) {
    if (std::abs(x) < 0.05f) return "0.0 dB";
    return juce::String(x, 1) + " dB";
}

CrumbsEditor::CrumbsEditor(CrumbsProcessor& p)
    : AudioProcessorEditor(p), proc(p) {
    setOpaque(true);
    setResizable(true, false);
    setResizeLimits(900, 600, 1800, 1200);
    setSize(1080, 700);
    startTimerHz(24);
}

CrumbsEditor::~CrumbsEditor() { stopTimer(); }

void CrumbsEditor::resized() { layout(); }

void CrumbsEditor::layout() {
    auto b = getLocalBounds().toFloat().reduced(20, 18);
    topBar = b.removeFromTop(34);
    b.removeFromTop(8);
    hintRow = b.removeFromTop(16);
    b.removeFromTop(8);
    const bool splitMode = proc.value(G_MODE) >= 0.5f;
    const float busH = 188.f;
    const float mixH = 86.f;
    if (splitMode) {
        // Orbs use the leftover height; mix + bus stay on the bottom.
        busRow = b.removeFromBottom(busH);
        b.removeFromBottom(12);
        mixRow = b.removeFromBottom(mixH);
        b.removeFromBottom(16);
        workArea = b;
    } else {
        // Pack Whole from the top so leftover space sits under the bus,
        // not as an empty band between the knobs and the crunch row.
        workArea = juce::Rectangle<float>(b.getX(), b.getY(), b.getWidth(), 30.f + 18.f + 88.f);
        auto after = b;
        after.removeFromTop(workArea.getHeight());
        after.removeFromTop(24);
        mixRow = after.removeFromTop(mixH);
        after.removeFromTop(16);
        busRow = after.removeFromTop(busH);
    }

    auto t = topBar;
    auto title = t.removeFromLeft(130);
    (void)title;
    t.removeFromLeft(16);
    modeWhole = t.removeFromLeft(78);
    t.removeFromLeft(4);
    modeSplit = t.removeFromLeft(78);
    t.removeFromLeft(14);
    btnAB = t.removeFromLeft(64);
    t.removeFromLeft(10);
    lookBox = t.removeFromLeft(110);
    t.removeFromLeft(10);
    latBox = t.removeFromLeft(90);
    t.removeFromRight(8);
    ceilLight = t.removeFromRight(18);
    t.removeFromRight(8);
    outMeter = t.removeFromRight(90);
    t.removeFromRight(8);
    inMeter = t.removeFromRight(90);

    // work: orbs on the left in Split, knobs fill the rest (or all of it in Whole)
    auto w = workArea;
    bool split = proc.value(G_MODE) >= 0.5f;
    orbs.clear();
    if (split) {
        orbArea = w.removeFromLeft(juce::jmax(280.f, w.getWidth() * 0.38f));
        w.removeFromLeft(16);
        auto o = orbArea;
        splitRow = o.removeFromBottom(56);
        o.removeFromBottom(10);
        float gap = 12.f;
        float ow = (o.getWidth() - gap) * 0.5f;
        float oh = (o.getHeight() - gap) * 0.5f;
        auto top = o.removeFromTop(oh);
        orbs.push_back(top.removeFromLeft(ow).reduced(4, 2));
        top.removeFromLeft(gap);
        orbs.push_back(top.reduced(4, 2));
        o.removeFromTop(gap);
        orbs.push_back(o.removeFromLeft(ow).reduced(4, 2));
        o.removeFromLeft(gap);
        orbs.push_back(o.reduced(4, 2));
        auto s = splitRow;
        lowSplitBox = s.removeFromLeft(s.getWidth() * 0.5f - 4);
        s.removeFromLeft(8);
        highSplitBox = s;
        knobArea = w;
        auto k = knobArea;
        auto flags = k.removeFromTop(28);
        listenBtn = flags.removeFromLeft(80);
        flags.removeFromLeft(8);
        muteBtn = flags.removeFromLeft(80);
        k.removeFromTop(14);
        auto knobRow = k.removeFromTop(88);
        partKnobs.clear();
        const int nk = 8;
        float gw = 6.f;
        float cw = (knobRow.getWidth() - gw * (nk - 1)) / nk;
        for (int i = 0; i < nk; ++i) {
            partKnobs.push_back(knobRow.removeFromLeft(cw));
            if (i + 1 < nk) knobRow.removeFromLeft(gw);
        }
        wholeKnobs.clear();
        wholePresets.clear();
    } else {
        orbArea = {};
        knobArea = w;
        auto k = knobArea;
        auto pre = k.removeFromTop(30);
        wholePresets.clear();
        const int np = 5;
        float gw = 8.f;
        float cw = (pre.getWidth() - gw * (np - 1)) / np;
        for (int i = 0; i < np; ++i) {
            wholePresets.push_back(pre.removeFromLeft(cw));
            if (i + 1 < np) pre.removeFromLeft(gw);
        }
        k.removeFromTop(18);
        auto knobRow = k.removeFromTop(88);
        wholeKnobs.clear();
        const int nk = 8;
        float gap = 8.f;
        float kw = (knobRow.getWidth() - gap * (nk - 1)) / nk;
        for (int i = 0; i < nk; ++i) {
            wholeKnobs.push_back(knobRow.removeFromLeft(kw));
            if (i + 1 < nk) knobRow.removeFromLeft(gap);
        }
        partKnobs.clear();
        lowSplitBox = {}; highSplitBox = {};
        listenBtn = {}; muteBtn = {};
    }

    // mix row: crunch type + mix/output/ceiling
    auto m = mixRow;
    crunchTypes.clear();
    auto ctypes = m.removeFromLeft(260);
    {
        juce::Rectangle<float> ct = ctypes;
        ct.removeFromTop(16); // "Crunch character" caption
        auto row = ct.removeFromTop(36);
        float gw = 6.f;
        float cw = (row.getWidth() - gw * 3) / 4;
        for (int i = 0; i < 4; ++i) {
            crunchTypes.push_back(row.removeFromLeft(cw));
            if (i < 3) row.removeFromLeft(gw);
        }
    }
    m.removeFromLeft(20);
    float gw = 12.f;
    float mw = (m.getWidth() - gw * 2) / 3;
    mixBox = m.removeFromLeft(mw); m.removeFromLeft(gw);
    outBox = m.removeFromLeft(mw); m.removeFromLeft(gw);
    ceilBox = m;

    // bus row — captions sit inside the row so they cannot overlap chips
    auto br = busRow;
    br.removeFromTop(16); // "Drum bus"
    br.removeFromTop(2);
    auto chips = br.removeFromTop(30);
    busChips.clear();
    const int nb = 5;
    float cg = 8.f;
    float cw = (chips.getWidth() - cg * (nb - 1)) / nb;
    for (int i = 0; i < nb; ++i) {
        busChips.push_back(chips.removeFromLeft(cw));
        if (i + 1 < nb) chips.removeFromLeft(cg);
    }
    br.removeFromTop(6);
    br.removeFromTop(14); // "Bus character" caption drawn in paint
    auto types = br.removeFromTop(24);
    busCrunchTypes.clear();
    types.removeFromLeft(4);
    for (int i = 0; i < 4; ++i) {
        busCrunchTypes.push_back(types.removeFromLeft(70));
        types.removeFromLeft(6);
    }
    br.removeFromTop(6);
    busKnobs.clear();
    const int bk = 5;
    float bg = 8.f;
    float bw = (br.getWidth() - bg * (bk - 1)) / bk;
    for (int i = 0; i < bk; ++i) {
        busKnobs.push_back(br.removeFromLeft(bw));
        if (i + 1 < bk) br.removeFromLeft(bg);
    }
}

void CrumbsEditor::paint(juce::Graphics& g) {
    layout();
    g.fillAll(beige());
    const bool split = proc.value(G_MODE) >= 0.5f;
    const bool ab = proc.value(G_AB) >= 0.5f;
    const bool look = proc.value(G_LOOKAHEAD) >= 0.5f;
    const int sel = proc.selectedPart();

    // Title
    g.setColour(ink());
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)).withExtraKerningFactor(0.14f));
    g.drawText("CRUMBS", topBar.withWidth(130), juce::Justification::centredLeft, false);

    chip(g, modeWhole, "Whole", !split);
    chip(g, modeSplit, "Split", split);
    chip(g, btnAB, ab ? "A/B ON" : "A/B", ab);
    chip(g, lookBox, look ? "Lookahead on" : "Lookahead off", look);

    int lat = proc.reportedLatency();
    double srate = std::max(1.0, proc.getSampleRate());
    juce::String latS = (lat <= 0) ? "0 ms" : juce::String(1000.0 * lat / srate, 2) + " ms";
    tiny(g, juce::Rectangle<float>(latBox.getX(), topBar.getY(), latBox.getWidth(), 12), "Latency", muted(), juce::Justification::centredLeft);
    g.setColour(ink());
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(latS, juce::Rectangle<float>(latBox.getX(), topBar.getY() + 12, latBox.getWidth(), 20),
               juce::Justification::centredLeft, false);

    auto meter = [&](juce::Rectangle<float> r, const juce::String& name, float pk) {
        tiny(g, r.removeFromTop(12), name, muted());
        g.setColour(wash());
        g.fillRoundedRectangle(r.reduced(0, 4), 3.f);
        float db = pk > 1e-8f ? gainToDb(pk) : -60.f;
        float t = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f);
        auto fill = r.reduced(0, 4).withWidth(std::max(2.f, r.getWidth() * t));
        g.setColour(pk >= dbToGain(proc.value(G_CEILING)) * 0.995f ? juce::Colour(0xffc4362a) : ink());
        g.fillRoundedRectangle(fill, 3.f);
    };
    meter(inMeter, "In", proc.inPeak());
    meter(outMeter, "Out", proc.outPeak());
    g.setColour(proc.ceilingHold() > 0.2f ? juce::Colour(0xffc4362a) : line());
    g.fillEllipse(ceilLight.reduced(2));

    juce::String hint = split
        ? "Split is a frequency split, not a stem separator. Parts will bleed. Use Listen to hear what each orb catches."
        : "Put this on a drum mixer track. Whole shapes the kit as one sound. Clean on the bus is a true bypass.";
    tiny(g, hintRow, hint, muted(), juce::Justification::centredLeft);

    if (split) {
        const char* names[] = {"Kick", "Snare", "Hats", "Perc"};
        for (int i = 0; i < kParts && i < (int)orbs.size(); ++i) {
            bool on = (i == sel);
            bool mutedP = proc.value(pp(i, P_MUTE)) >= 0.5f;
            bool lis = proc.value(pp(i, P_LISTEN)) >= 0.5f;
            auto cell = orbs[(size_t)i];
            auto lab = cell.removeFromBottom(36);
            drawOrb(g, cell, partCol(i), mutedP ? 0.22f : (on ? 0.70f : 0.48f), on);
            auto nameR = lab.removeFromTop(16);
            tiny(g, nameR, names[i], on ? ink() : muted());
            if (lis || mutedP) {
                juce::String flag;
                if (lis) flag = "Listen";
                if (mutedP) flag = flag.isEmpty() ? "Mute" : flag + "  +  mute";
                tiny(g, lab, flag, on ? ink() : muted());
            }
        }
        knob(g, lowSplitBox, "Low split", fmtHz(proc.value(S_LOW)), ink());
        knob(g, highSplitBox, "High split", fmtHz(proc.value(S_HIGH)), ink());
        chip(g, listenBtn, proc.value(pp(sel, P_LISTEN)) >= 0.5f ? "Listening" : "Listen",
             proc.value(pp(sel, P_LISTEN)) >= 0.5f);
        chip(g, muteBtn, proc.value(pp(sel, P_MUTE)) >= 0.5f ? "Muted" : "Mute",
             proc.value(pp(sel, P_MUTE)) >= 0.5f);

        const int ids[] = { pp(sel,P_LEVEL), pp(sel,P_PUNCH), pp(sel,P_BODY), pp(sel,P_DECAY),
                            pp(sel,P_CRUNCH), pp(sel,P_PAN), pp(sel,P_WIDTH), pp(sel,P_SENS) };
        const char* labs[] = {"Level","Punch","Body","Decay","Crunch","Pan","Width","Sensitivity"};
        for (int i = 0; i < 8 && i < (int)partKnobs.size(); ++i) {
            float v = proc.value(ids[i]);
            juce::String s = (i == 2 || i == 5) ? fmtSigned(v) : fmt01(v);
            knob(g, partKnobs[(size_t)i], labs[i], s, partCol(sel));
        }
    } else {
        for (int i = 0; i < (int)wholePresets.size(); ++i)
            chip(g, wholePresets[(size_t)i], wholePresetName(i), (int)std::lround(proc.value(W_PRESET)) == i);
        const int ids[] = { W_PUNCH, W_BODY, W_SNAP, W_CRUNCH, W_TONE, W_GLUE, W_ROOM, W_WIDTH };
        const char* labs[] = {"Punch","Body","Snap","Crunch","Tone","Glue","Room","Width"};
        for (int i = 0; i < 8 && i < (int)wholeKnobs.size(); ++i) {
            float v = proc.value(ids[i]);
            juce::String s = (i == 1 || i == 4) ? fmtSigned(v) : fmt01(v);
            knob(g, wholeKnobs[(size_t)i], labs[i], s, ink());
        }
    }

    tiny(g, juce::Rectangle<float>(crunchTypes.empty() ? 0 : crunchTypes[0].getX(),
                                   mixRow.getY(), 200, 16), "Crunch character", muted(), juce::Justification::centredLeft);
    const char* cts[] = {"Tape","Tube","Clip","Fuzz"};
    int ct = (int)std::lround(proc.value(W_CRTYPE));
    for (int i = 0; i < (int)crunchTypes.size(); ++i)
        chip(g, crunchTypes[(size_t)i], cts[i], i == ct);

    knob(g, mixBox, "Mix", juce::String((int)std::lround(proc.value(G_MIX) * 100)) + " %", ink());
    knob(g, outBox, "Output", fmtDb(proc.value(G_OUTPUT)), ink());
    knob(g, ceilBox, "Ceiling", fmtDb(proc.value(G_CEILING)), ink());

    // bus
    tiny(g, juce::Rectangle<float>(busRow.getX(), busRow.getY(), 200, 14), "Drum bus", muted(), juce::Justification::centredLeft);
    int bp = (int)std::lround(proc.value(B_BUS));
    for (int i = 0; i < (int)busChips.size(); ++i)
        chip(g, busChips[(size_t)i], busPresetName(i), i == bp);
    if (!busCrunchTypes.empty()) {
        tiny(g, juce::Rectangle<float>(busCrunchTypes[0].getX(),
                                       busCrunchTypes[0].getY() - 14,
                                       200, 14),
             "Bus character", muted(), juce::Justification::centredLeft);
    }
    int bct = (int)std::lround(proc.value(B_CRTYPE));
    for (int i = 0; i < (int)busCrunchTypes.size(); ++i)
        chip(g, busCrunchTypes[(size_t)i], cts[i], i == bct);
    const int bids[] = { B_PUNCH, B_CRUNCH, B_SQUASH, B_PUMP, B_ROOM };
    const char* blabs[] = {"Punch","Crunch","Squash","Pump","Room"};
    for (int i = 0; i < 5 && i < (int)busKnobs.size(); ++i)
        knob(g, busKnobs[(size_t)i], blabs[i], fmt01(proc.value(bids[i])), ink());
}

void CrumbsEditor::timerCallback() { repaint(); }

void CrumbsEditor::dragKnob(int param, float startVal, float dy, float sensitivity) {
    if (auto* prm = proc.param(param)) {
        float rng = prm->getNormalisableRange().end - prm->getNormalisableRange().start;
        float v = startVal - dy * (rng / sensitivity);
        v = prm->getNormalisableRange().snapToLegalValue(v);
        prm->setValueNotifyingHost(prm->convertTo0to1(v));
    }
}

void CrumbsEditor::mouseDown(const juce::MouseEvent& e) {
    layout();
    auto p = e.position;
    dragParam = -1;
    const bool split = proc.value(G_MODE) >= 0.5f;

    if (modeWhole.contains(p)) { proc.setValue(G_MODE, 0); return; }
    if (modeSplit.contains(p)) { proc.setValue(G_MODE, 1); return; }
    if (btnAB.contains(p)) { proc.setValue(G_AB, proc.value(G_AB) >= 0.5f ? 0.f : 1.f); return; }
    if (lookBox.contains(p)) { proc.setValue(G_LOOKAHEAD, proc.value(G_LOOKAHEAD) >= 0.5f ? 0.f : 1.f); return; }

    for (int i = 0; i < (int)orbs.size(); ++i) if (orbs[(size_t)i].contains(p)) {
        proc.setSelectedPart(i);
        if (e.mods.isAltDown())
            proc.setValue(pp(i, P_LISTEN), proc.value(pp(i, P_LISTEN)) >= 0.5f ? 0.f : 1.f);
        return;
    }
    if (split) {
        if (listenBtn.contains(p)) {
            int s = proc.selectedPart();
            proc.setValue(pp(s, P_LISTEN), proc.value(pp(s, P_LISTEN)) >= 0.5f ? 0.f : 1.f);
            return;
        }
        if (muteBtn.contains(p)) {
            int s = proc.selectedPart();
            proc.setValue(pp(s, P_MUTE), proc.value(pp(s, P_MUTE)) >= 0.5f ? 0.f : 1.f);
            return;
        }
    }
    for (int i = 0; i < (int)wholePresets.size(); ++i) if (wholePresets[(size_t)i].contains(p)) {
        proc.applyWholePreset(i); return;
    }
    for (int i = 0; i < (int)busChips.size(); ++i) if (busChips[(size_t)i].contains(p)) {
        proc.applyBusPreset(i); return;
    }
    for (int i = 0; i < (int)crunchTypes.size(); ++i) if (crunchTypes[(size_t)i].contains(p)) {
        proc.setValue(W_CRTYPE, (float)i); return;
    }
    for (int i = 0; i < (int)busCrunchTypes.size(); ++i) if (busCrunchTypes[(size_t)i].contains(p)) {
        proc.setValue(B_CRTYPE, (float)i); return;
    }

    auto begin = [&](int id) {
        dragParam = id; dragStart = proc.value(id); dragY0 = p.y;
        if (auto* prm = proc.param(id)) prm->beginChangeGesture();
    };
    if (!split) {
        const int ids[] = { W_PUNCH, W_BODY, W_SNAP, W_CRUNCH, W_TONE, W_GLUE, W_ROOM, W_WIDTH };
        for (int i = 0; i < (int)wholeKnobs.size() && i < 8; ++i) if (wholeKnobs[(size_t)i].contains(p)) { begin(ids[i]); return; }
    } else {
        int s = proc.selectedPart();
        const int ids[] = { pp(s,P_LEVEL), pp(s,P_PUNCH), pp(s,P_BODY), pp(s,P_DECAY),
                            pp(s,P_CRUNCH), pp(s,P_PAN), pp(s,P_WIDTH), pp(s,P_SENS) };
        for (int i = 0; i < (int)partKnobs.size() && i < 8; ++i) if (partKnobs[(size_t)i].contains(p)) { begin(ids[i]); return; }
        if (lowSplitBox.contains(p)) { begin(S_LOW); return; }
        if (highSplitBox.contains(p)) { begin(S_HIGH); return; }
    }
    if (mixBox.contains(p)) { begin(G_MIX); return; }
    if (outBox.contains(p)) { begin(G_OUTPUT); return; }
    if (ceilBox.contains(p)) { begin(G_CEILING); return; }
    const int bids[] = { B_PUNCH, B_CRUNCH, B_SQUASH, B_PUMP, B_ROOM };
    for (int i = 0; i < (int)busKnobs.size() && i < 5; ++i) if (busKnobs[(size_t)i].contains(p)) { begin(bids[i]); return; }
}

void CrumbsEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragParam < 0) return;
    dragKnob(dragParam, dragStart, e.position.y - dragY0);
}

void CrumbsEditor::mouseUp(const juce::MouseEvent&) {
    if (dragParam >= 0) if (auto* prm = proc.param(dragParam)) prm->endChangeGesture();
    dragParam = -1;
}

void CrumbsEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    // Wheel over a knob nudges it.
    layout();
    auto p = e.position;
    auto tryWheel = [&](int id) {
        if (auto* prm = proc.param(id)) {
            float rng = prm->getNormalisableRange().end - prm->getNormalisableRange().start;
            float v = proc.value(id) + w.deltaY * rng * 0.05f;
            proc.setValue(id, prm->getNormalisableRange().snapToLegalValue(v));
        }
    };
    if (mixBox.contains(p)) { tryWheel(G_MIX); return; }
    if (outBox.contains(p)) { tryWheel(G_OUTPUT); return; }
    if (ceilBox.contains(p)) { tryWheel(G_CEILING); return; }
}

