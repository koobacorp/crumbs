#include "PluginEditor.h"
using namespace cr;

static juce::Colour beige() { return juce::Colour(0xffe6d4b5); }
static juce::Colour ink() { return juce::Colour(0xff2a2420); }
static juce::Colour muted() { return juce::Colour(0xff7a7268); }
static juce::Colour wash() { return juce::Colour(0xffd4c3a4); }
static juce::Colour line() { return juce::Colour(0xffc4b399); }
static juce::Colour palCol(int i) { return juce::Colour(kPalette[(i % 8 + 8) % 8]); }

static void drawOrb(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float alpha = 0.55f) {
    auto cx = r.getCentre();
    float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.42f;
    juce::ColourGradient gr(c.withAlpha(alpha), cx.x, cx.y,
                            c.withAlpha(0.0f), cx.x + rad, cx.y, true);
    g.setGradientFill(gr);
    g.fillEllipse(cx.x - rad, cx.y - rad, rad * 2, rad * 2);
    // softer outer bloom
    g.setColour(c.withAlpha(alpha * 0.18f));
    g.fillEllipse(cx.x - rad * 1.45f, cx.y - rad * 1.45f, rad * 2.9f, rad * 2.9f);
}

static void tiny(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c, juce::Justification j = juce::Justification::centred) {
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(10.0f)).withExtraKerningFactor(0.12f));
    g.drawText(t.toUpperCase(), r, j, false);
}

static void valueFont(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c) {
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText(t, r, juce::Justification::centred, false);
}

CrumbsEditor::CrumbsEditor(CrumbsProcessor& p)
    : AudioProcessorEditor(p), proc(p) {
    setOpaque(true);
    setResizable(true, false);
    setResizeLimits(860, 560, 1800, 1200);
    setSize(1080, 700);
    startTimerHz(24);
}

CrumbsEditor::~CrumbsEditor() { stopTimer(); }

void CrumbsEditor::layout() {
    auto b = getLocalBounds().toFloat().reduced(18, 16);
    topBar = b.removeFromTop(28);
    b.removeFromTop(8);
    timeline = b.removeFromTop(100);
    b.removeFromTop(10);
    laneBar = b.removeFromTop(36);
    b.removeFromTop(8);
    busRow = b.removeFromBottom(86);
    b.removeFromBottom(8);
    fineRow = b.removeFromBottom(96);
    b.removeFromBottom(6);
    orbArea = b;

    auto t = topBar;
    btnKeep = t.removeFromRight(100);
    t.removeFromRight(8);
    snapBox = t.removeFromRight(88);
    t.removeFromRight(6);
    scaleBox = t.removeFromRight(96);
    t.removeFromRight(6);
    keyBox = t.removeFromRight(70);
    t.removeFromRight(16);
    btnRedo = t.removeFromRight(56);
    t.removeFromRight(4);
    btnUndo = t.removeFromRight(56);
    t.removeFromRight(8);
    btnAB = t.removeFromRight(50);

    auto lb = laneBar;
    modeSamp = lb.removeFromRight(78);
    modeLoop = lb.removeFromRight(70);
    lb.removeFromRight(10);
    btnPlay = lb.removeFromRight(64);
    lb.removeFromRight(8);
    btnDragM = lb.removeFromRight(78);
    btnDragA = lb.removeFromRight(78);
    btnSaveM = lb.removeFromRight(70);
    btnSaveA = lb.removeFromRight(78);
    lb.removeFromRight(8);
    btnAdd = lb.removeFromRight(36);
    // remaining width: lane chips
    laneChips.clear();
    int nL = (int)proc.machine().lanes.size();
    float cw = nL > 0 ? (lb.getWidth() - 6 * (nL - 1)) / juce::jmax(1, nL) : 80;
    for (int i = 0; i < nL; ++i) {
        laneChips.push_back(lb.removeFromLeft(cw));
        lb.removeFromLeft(6);
    }

    // rebuild + fine knobs depend on selected lane / rebuilt
    knobs.clear();
    int sel = juce::jlimit(0, juce::jmax(0, (int)proc.machine().lanes.size() - 1), proc.machine().selectedLane);
    bool reb = sel < (int)proc.machine().lanes.size() && proc.machine().lanes[(size_t)sel].rebuilt;
    int kind = sel < (int)proc.machine().lanes.size() ? proc.machine().lanes[(size_t)sel].kind : 0;
    std::vector<std::pair<juce::String, int>> big, fine;
    if (reb) {
        if (kind == LANE_KICK) {
            big = {{"Punch", lp(sel, LP_KPUNCH)}, {"Boom", lp(sel, LP_KBOOM)}, {"Tune", lp(sel, LP_KTUNE)}, {"Crunch", lp(sel, LP_KCRUNCH)}};
        } else if (kind == LANE_SNARE) {
            big = {{"Body", lp(sel, LP_KBODY)}, {"Snap", lp(sel, LP_KSNAP)}, {"Tune", lp(sel, LP_KTUNE)}, {"Crunch", lp(sel, LP_KCRUNCH)}};
        } else if (kind == LANE_HATS) {
            big = {{"Tone", lp(sel, LP_TONE)}, {"Length", lp(sel, LP_KLENGTH)}, {"Shimmer", lp(sel, LP_KSHIMMER)}, {"Crunch", lp(sel, LP_KCRUNCH)}};
        } else {
            big = {{"Body", lp(sel, LP_KBODY)}, {"Tune", lp(sel, LP_KTUNE)}, {"Length", lp(sel, LP_KLENGTH)}, {"Crunch", lp(sel, LP_KCRUNCH)}};
        }
        big.push_back({"Blend", lp(sel, LP_BLEND)});
    } else {
        // before rebuild, still show the everyday lane knobs large-ish in the orb area
        big = {{"Level", lp(sel, LP_LEVEL)}, {"Pitch", lp(sel, LP_PITCH)}, {"Decay", lp(sel, LP_DECAY)}, {"Crunch", lp(sel, LP_CRUNCH)}};
    }
    fine = {{"Level", lp(sel, LP_LEVEL)}, {"Pitch", lp(sel, LP_PITCH)}, {"Decay", lp(sel, LP_DECAY)},
            {"Tone", lp(sel, LP_TONE)}, {"Crunch", lp(sel, LP_CRUNCH)}, {"Pan", lp(sel, LP_PAN)},
            {"Room", lp(sel, LP_ROOM)}, {"Swing", lp(sel, LP_SWING)}, {"Humanize", lp(sel, LP_HUMAN)},
            {"Ghosts", lp(sel, LP_GHOSTS)}};
    auto place = [&](std::vector<std::pair<juce::String,int>>& src, juce::Rectangle<float> area, bool rebuildSet) {
        int n = (int)src.size(); if (n == 0) return;
        float w = area.getWidth() / n;
        for (int i = 0; i < n; ++i) {
            auto r = area.withX(area.getX() + i * w).withWidth(w).reduced(8, 4);
            knobs.push_back({src[(size_t)i].first, src[(size_t)i].second, r, rebuildSet});
        }
    };
    auto mid = orbArea;
    btnRebuild = mid.removeFromTop(26).removeFromLeft(100);
    mid.removeFromTop(4);
    // palette + split/merge/rename on the right of rebuild row
    auto tools = orbArea.withHeight(26).withX(btnRebuild.getRight() + 8);
    btnSplit = tools.removeFromLeft(70); tools.removeFromLeft(6);
    btnMerge = tools.removeFromLeft(70); tools.removeFromLeft(6);
    btnRename = tools.removeFromLeft(78);
    palSwatches.clear();
    tools.removeFromLeft(12);
    for (int i = 0; i < 8; ++i) { palSwatches.push_back(tools.removeFromLeft(16).withHeight(16).translated(0, 5)); tools.removeFromLeft(4); }

    place(big, mid, reb);
    place(fine, fineRow, false);

    auto br = busRow;
    auto labs = br.removeFromTop(20);
    (void)labs;
    auto presets = br.removeFromTop(28);
    busBtns.clear();
    float bw = presets.getWidth() / 5.f;
    for (int i = 0; i < 5; ++i) busBtns.push_back(presets.removeFromLeft(bw).reduced(3, 2));
    auto ctrls = br;
    std::vector<std::pair<juce::String,int>> busK = {{"Punch", G_PUNCH},{"Crunch", G_CRUNCH},{"Squash", G_SQUASH},{"Pump", G_PUMP},{"Room", G_BROOM}};
    float cw2 = ctrls.getWidth() / 6.f;
    for (int i = 0; i < 5; ++i) {
        auto r = ctrls.removeFromLeft(cw2).reduced(6, 0);
        knobs.push_back({busK[(size_t)i].first, busK[(size_t)i].second, r, false});
    }
    auto typ = ctrls;
    crunchTypes.clear();
    float tw = typ.getWidth() / 4.f;
    for (int i = 0; i < 4; ++i) crunchTypes.push_back(typ.removeFromLeft(tw).reduced(2, 8));
}

void CrumbsEditor::resized() { layout(); }

void CrumbsEditor::timerCallback() { repaint(); }

void CrumbsEditor::paint(juce::Graphics& g) {
    layout();
    g.fillAll(beige());
    auto& m = proc.machine();
    int sel = juce::jlimit(0, juce::jmax(0, (int)m.lanes.size() - 1), m.selectedLane);
    juce::Colour laneC = m.lanes.empty() ? muted() : palCol(m.lanes[(size_t)sel].colour);

    // corner catalogue type
    tiny(g, juce::Rectangle<float>(18, 8, 140, 14), "CRUMBS", muted(), juce::Justification::centredLeft);
    tiny(g, juce::Rectangle<float>((float)getWidth() - 160, 8, 142, 14), "V1.0  KOOBACORP", muted(), juce::Justification::centredRight);

    // top controls
    auto drawBtn = [&](juce::Rectangle<float> r, const juce::String& t, bool on, bool hi = false) {
        g.setColour(on ? ink() : wash());
        g.fillRoundedRectangle(r, 3.f);
        g.setColour(on ? beige() : (hi ? ink() : muted()));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(t.toUpperCase(), r, juce::Justification::centred, false);
    };
    auto drawBox = [&](juce::Rectangle<float> r, const juce::String& cap, const juce::String& val) {
        tiny(g, r.removeFromTop(11), cap, muted());
        g.setColour(ink());
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(val, r, juce::Justification::centred, false);
    };
    const char* keys[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const char* scales[] = {"Major","Minor","Chromatic","Maj Pent","Min Pent"};
    const char* snaps[] = {"Nearest","Root"};
    drawBox(keyBox, "Key", keys[juce::jlimit(0, 11, m.keyPc)]);
    drawBox(scaleBox, "Scale", scales[juce::jlimit(0, 4, m.scale)]);
    drawBox(snapBox, "Snap", snaps[m.snapToRoot ? 1 : 0]);
    drawBtn(btnKeep, "Keep in key", m.keepInKey, true);
    drawBtn(btnAB, "A/B", m.abOriginal, true);
    drawBtn(btnUndo, "Undo", false, m.canUndo());
    drawBtn(btnRedo, "Redo", false, m.canRedo());

    // note readout (only once a loop is loaded)
    if (proc.hasLoop() && !m.lanes.empty()) {
        float hz = m.laneDisplayHz(sel);
        auto nc = noteCentsFromHz(hz);
        juce::String note = formatNoteCents(nc);
        if (m.lanes[(size_t)sel].kind == LANE_HATS || !nc.valid)
            note = "NO PITCH";
        g.setColour(ink());
        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        auto nr = topBar.withWidth(220).translated(80, 0);
        g.drawText(note, nr, juce::Justification::centredLeft, false);
        tiny(g, nr.translated(0, 1).removeFromRight(90), "CONCERT PITCH", muted(), juce::Justification::centredRight);
    }

    // empty state
    if (m.original.n() <= 0) {
        drawOrb(g, getLocalBounds().toFloat(), juce::Colour(0xffcbb892), 0.45f);
        tiny(g, juce::Rectangle<float>(0, (float)getHeight() * 0.62f, (float)getWidth(), 20),
             "Drop a loop", muted());
        return;
    }

    // timeline
    g.setColour(wash());
    g.fillRoundedRectangle(timeline, 6.f);
    int N = juce::jmax(1, m.arrangedSamples);
    for (int i = 0; i < (int)m.events.size(); ++i) {
        auto& e = m.events[(size_t)i];
        if (e.lane < 0 || e.lane >= (int)m.lanes.size()) continue;
        float x0 = timeline.getX() + timeline.getWidth() * e.destStart / (float)N;
        int len = juce::jmax(e.srcEnd - e.srcStart, (int)(0.03 * (m.original.sr > 0 ? m.original.sr : 44100)));
        float x1 = timeline.getX() + timeline.getWidth() * (e.destStart + len) / (float)N;
        auto r = juce::Rectangle<float>(x0, timeline.getY() + 10, juce::jmax(4.f, x1 - x0), timeline.getHeight() - 20);
        auto c = palCol(m.lanes[(size_t)e.lane].colour);
        if (i == m.selectedEvent) {
            g.setColour(c.withAlpha(0.28f));
            g.fillRoundedRectangle(r.expanded(3, 4), 4.f);
        }
        g.setColour(c);
        g.fillRoundedRectangle(r, 3.f);
        if (i == m.selectedEvent) {
            g.setColour(ink());
            g.drawRoundedRectangle(r, 3.f, 1.4f);
        }
    }
    tiny(g, timeline.withHeight(12).translated(6, 2), "Arrange  ·  drag to move  ·  double-click to delete", muted(), juce::Justification::centredLeft);

    // lane chips
    for (int i = 0; i < (int)laneChips.size(); ++i) {
        auto r = laneChips[(size_t)i];
        auto c = palCol(m.lanes[(size_t)i].colour);
        bool on = (i == sel);
        g.setColour(on ? c : c.withAlpha(0.22f));
        g.fillRoundedRectangle(r, 4.f);
        g.setColour(on ? juce::Colours::white : ink());
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(juce::String(m.lanes[(size_t)i].name).toUpperCase(), r, juce::Justification::centred, true);
    }
    drawBtn(btnAdd, "+", false, (int)m.lanes.size() < 8);
    drawBtn(btnSaveA, "Save audio", false, true);
    drawBtn(btnSaveM, "Save MIDI", false, true);
    drawBtn(btnDragA, "Drag audio", false, true);
    drawBtn(btnDragM, "Drag MIDI", false, true);
    drawBtn(btnPlay, proc.isPreview() ? "Stop" : "Play", proc.isPreview(), true);
    drawBtn(modeLoop, "Loop", m.playMode == PLAY_LOOP, true);
    drawBtn(modeSamp, "Sampler", m.playMode == PLAY_SAMPLER, true);

    // orb behind knobs
    drawOrb(g, orbArea, laneC, 0.55f);
    drawBtn(btnRebuild, m.lanes[(size_t)sel].rebuilt ? "Rebuilt" : "Rebuild", m.lanes[(size_t)sel].rebuilt, true);
    drawBtn(btnSplit, "Split", false, (int)m.lanes.size() < 8);
    drawBtn(btnMerge, "Merge", false, (int)m.lanes.size() > 1);
    drawBtn(btnRename, "Rename", false, true);
    for (int i = 0; i < (int)palSwatches.size(); ++i) {
        g.setColour(palCol(i));
        g.fillEllipse(palSwatches[(size_t)i]);
        if (m.lanes[(size_t)sel].colour == i) {
            g.setColour(ink());
            g.drawEllipse(palSwatches[(size_t)i].expanded(2, 2), 1.4f);
        }
    }

    auto drawKnob = [&](const KnobInfo& k) {
        auto r = k.r;
        auto cap = r.removeFromTop(14);
        tiny(g, cap, k.label, muted());
        auto valR = r.removeFromBottom(16);
        auto* prm = proc.param(k.param);
        float nrm = prm ? prm->getValue() : 0;
        float real = prm ? prm->convertFrom0to1(nrm) : 0;
        juce::String vs = prm ? prm->getCurrentValueAsText() : "-";
        // compact numeric
        if (k.param == lp(sel, LP_PITCH) || k.param == lp(sel, LP_KTUNE) || k.param == G_CRTYPE) {}
        valueFont(g, valR, vs, ink());
        float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.42f;
        auto cx = r.getCentre();
        g.setColour(line());
        g.drawEllipse(cx.x - rad, cx.y - rad, rad * 2, rad * 2, 1.4f);
        float a0 = juce::MathConstants<float>::pi * 0.75f;
        float a1 = juce::MathConstants<float>::pi * 2.25f;
        float a = a0 + nrm * (a1 - a0);
        juce::Path arc;
        arc.addCentredArc(cx.x, cx.y, rad, rad, 0, a0, a, true);
        g.setColour(k.rebuild ? laneC : ink());
        g.strokePath(arc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(ink());
        g.drawLine(cx.x, cx.y, cx.x + rad * 0.78f * std::cos(a), cx.y + rad * 0.78f * std::sin(a), 2.f);
    };
    // only fine + big + bus knobs already in knobs[]
    for (auto& k : knobs) drawKnob(k);

    // bus
    tiny(g, busRow.withHeight(14).translated(0, -16), "Drum bus", muted(), juce::Justification::centredLeft);
    for (int i = 0; i < (int)busBtns.size(); ++i) {
        bool on = (m.bus.preset == i);
        g.setColour(on ? ink() : wash());
        g.fillRoundedRectangle(busBtns[(size_t)i], 3.f);
        g.setColour(on ? beige() : ink());
        g.setFont(juce::Font(juce::FontOptions(11.0f, on ? juce::Font::bold : juce::Font::plain)));
        g.drawText(juce::String(busPresetName(i)).toUpperCase(), busBtns[(size_t)i], juce::Justification::centred, true);
    }
    const char* cts[] = {"Tape","Tube","Clip","Fuzz"};
    for (int i = 0; i < (int)crunchTypes.size(); ++i) {
        bool on = (m.bus.crunchType == i);
        g.setColour(on ? ink().withAlpha(0.85f) : wash());
        g.fillRoundedRectangle(crunchTypes[(size_t)i], 3.f);
        tiny(g, crunchTypes[(size_t)i], cts[i], on ? beige() : muted());
    }

    if (toast.isNotEmpty() && juce::Time::getMillisecondCounter() < toastUntil) {
        auto tr = juce::Rectangle<float>((float)getWidth() * 0.5f - 180, (float)getHeight() - 36, 360, 22);
        g.setColour(ink().withAlpha(0.85f));
        g.fillRoundedRectangle(tr, 3.f);
        tiny(g, tr, toast, beige());
    }
}

CrumbsEditor::Hit CrumbsEditor::hitTest(juce::Point<float> p) const {
    Hit h;
    auto inside = [&](juce::Rectangle<float> r) { return r.contains(p); };
    if (inside(btnAB)) { h.kind = Hit::Button; h.index = 1; return h; }
    if (inside(btnUndo)) { h.kind = Hit::Button; h.index = 2; return h; }
    if (inside(btnRedo)) { h.kind = Hit::Button; h.index = 3; return h; }
    if (inside(btnKeep)) { h.kind = Hit::Button; h.index = 4; return h; }
    if (inside(btnPlay)) { h.kind = Hit::Button; h.index = 5; return h; }
    if (inside(btnRebuild)) { h.kind = Hit::Button; h.index = 6; return h; }
    if (inside(btnAdd)) { h.kind = Hit::Button; h.index = 7; return h; }
    if (inside(btnSplit)) { h.kind = Hit::Button; h.index = 8; return h; }
    if (inside(btnMerge)) { h.kind = Hit::Button; h.index = 9; return h; }
    if (inside(btnSaveA)) { h.kind = Hit::Button; h.index = 10; return h; }
    if (inside(btnSaveM)) { h.kind = Hit::Button; h.index = 11; return h; }
    if (inside(btnDragA)) { h.kind = Hit::Button; h.index = 12; return h; }
    if (inside(btnDragM)) { h.kind = Hit::Button; h.index = 13; return h; }
    if (inside(btnRename)) { h.kind = Hit::Button; h.index = 14; return h; }
    if (inside(modeLoop)) { h.kind = Hit::Mode; h.index = 0; return h; }
    if (inside(modeSamp)) { h.kind = Hit::Mode; h.index = 1; return h; }
    if (inside(keyBox)) { h.kind = Hit::Button; h.index = 20; return h; }
    if (inside(scaleBox)) { h.kind = Hit::Button; h.index = 21; return h; }
    if (inside(snapBox)) { h.kind = Hit::Button; h.index = 22; return h; }
    for (int i = 0; i < (int)laneChips.size(); ++i) if (inside(laneChips[(size_t)i])) { h.kind = Hit::LaneChip; h.index = i; return h; }
    for (int i = 0; i < (int)busBtns.size(); ++i) if (inside(busBtns[(size_t)i])) { h.kind = Hit::Bus; h.index = i; return h; }
    for (int i = 0; i < (int)palSwatches.size(); ++i) if (inside(palSwatches[(size_t)i])) { h.kind = Hit::Palette; h.index = i; return h; }
    for (int i = 0; i < (int)crunchTypes.size(); ++i) if (inside(crunchTypes[(size_t)i])) { h.kind = Hit::CrunchType; h.index = i; return h; }
    for (int i = 0; i < (int)knobs.size(); ++i) if (inside(knobs[(size_t)i].r)) { h.kind = Hit::Knob; h.index = i; return h; }
    if (inside(timeline) && proc.hasLoop()) {
        auto& m = proc.machine();
        int N = juce::jmax(1, m.arrangedSamples);
        int best = -1; float bestD = 1e9f;
        for (int i = 0; i < (int)m.events.size(); ++i) {
            auto& e = m.events[(size_t)i];
            float x0 = timeline.getX() + timeline.getWidth() * e.destStart / (float)N;
            int len = juce::jmax(e.srcEnd - e.srcStart, 64);
            float x1 = timeline.getX() + timeline.getWidth() * (e.destStart + len) / (float)N;
            if (p.x >= x0 - 2 && p.x <= x1 + 2 && p.y >= timeline.getY() && p.y <= timeline.getBottom()) {
                float d = std::abs(p.x - 0.5f * (x0 + x1));
                if (d < bestD) { bestD = d; best = i; }
            }
        }
        if (best >= 0) { h.kind = Hit::Slice; h.index = best; return h; }
    }
    return h;
}

void CrumbsEditor::mouseDown(const juce::MouseEvent& ev) {
    lastMouse = ev.position;
    auto h = hitTest(ev.position);
    auto& m = proc.machine();
    dragSlice = dragKnob = -1;
    gesture = false;
    if (h.kind == Hit::LaneChip) { m.setLane(h.index); proc.markDirty(); repaint(); return; }
    if (h.kind == Hit::Bus) {
        proc.setValue(G_BUS, (float)h.index);
        m.bus.applyPreset(h.index);
        proc.pushMachineToParams();
        proc.markDirty(); proc.rebuildNow(); repaint(); return;
    }
    if (h.kind == Hit::CrunchType) { proc.setValue(G_CRTYPE, (float)h.index); proc.markDirty(); return; }
    if (h.kind == Hit::Palette) {
        if (!m.lanes.empty()) { m.recolourLane(m.selectedLane, h.index); proc.markDirty(); }
        return;
    }
    if (h.kind == Hit::Mode) { proc.setValue(G_MODE, (float)h.index); return; }
    if (h.kind == Hit::Knob) {
        dragKnob = h.index;
        auto* prm = proc.param(knobs[(size_t)dragKnob].param);
        dragStartVal = prm ? prm->getValue() : 0;
        dragStartY = ev.position.y;
        if (prm) prm->beginChangeGesture();
        gesture = true;
        return;
    }
    if (h.kind == Hit::Slice) {
        m.selectedEvent = h.index;
        m.selectedLane = m.events[(size_t)h.index].lane;
        if (ev.mods.isPopupMenu()) {
            juce::PopupMenu menu;
            for (int i = 0; i < (int)m.lanes.size(); ++i)
                menu.addItem(100 + i, "Send to " + juce::String(m.lanes[(size_t)i].name), true, m.events[(size_t)h.index].lane == i);
            menu.addSeparator();
            menu.addItem(1, "Delete");
            menu.addItem(2, "Copy");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, idx = h.index](int r) {
                auto& mm = proc.machine();
                if (r == 1) mm.deleteEvent(idx);
                else if (r == 2) mm.copyEvent(idx);
                else if (r >= 100) mm.fixSort(idx, r - 100);
                proc.markDirty(); proc.rebuildNow();
            });
            return;
        }
        dragSlice = h.index;
        dragStartDest = m.events[(size_t)h.index].destStart;
        m.beginGesture();
        return;
    }
    if (h.kind == Hit::Button) {
        switch (h.index) {
            case 1: proc.setValue(G_AB, m.abOriginal ? 0.f : 1.f); break;
            case 2: m.doUndo(); proc.pushMachineToParams(); proc.markDirty(); proc.rebuildNow(); break;
            case 3: m.doRedo(); proc.pushMachineToParams(); proc.markDirty(); proc.rebuildNow(); break;
            case 4: proc.setValue(G_KEEPKEY, m.keepInKey ? 0.f : 1.f); break;
            case 5: proc.setPreview(!proc.isPreview()); break;
            case 6:
                m.rebuildLane(m.selectedLane);
                proc.pushMachineToParams(); proc.markDirty(); proc.rebuildNow();
                break;
            case 7:
                if (m.addLane()) { proc.pushMachineToParams(); proc.markDirty(); }
                break;
            case 8:
                if (!m.lanes.empty()) {
                    auto& L = m.lanes[(size_t)m.selectedLane];
                    juce::String a = L.name, b = a + " B";
                    if (L.kind == LANE_HATS) { a = "Closed Hats"; b = "Open Hats"; }
                    if (L.kind == LANE_KICK) { a = "Kick"; b = "808"; }
                    m.splitLane(m.selectedLane, a.toStdString(), b.toStdString(), L.kind);
                    proc.pushMachineToParams(); proc.markDirty(); proc.rebuildNow();
                }
                break;
            case 9:
                if ((int)m.lanes.size() >= 2)
                    m.mergeLanes(m.selectedLane, juce::jmin(m.selectedLane + 1, (int)m.lanes.size() - 1));
                proc.pushMachineToParams(); proc.markDirty(); proc.rebuildNow();
                break;
            case 10: {
                auto chooser = std::make_shared<juce::FileChooser>("Save audio", juce::File(), "*.wav");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, chooser](const juce::FileChooser& c) {
                    auto f = c.getResult(); if (f != juce::File()) proc.saveAudio(f);
                });
            } break;
            case 11: {
                auto chooser = std::make_shared<juce::FileChooser>("Save MIDI", juce::File(), "*.mid");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, chooser](const juce::FileChooser& c) {
                    auto f = c.getResult(); if (f != juce::File()) proc.saveMidi(f);
                });
            } break;
            case 12: {
                auto f = proc.writeTempAudio(-1);
                if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({f.getFullPathName()}, false);
            } break;
            case 13: {
                auto f = proc.writeTempMidi();
                if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({f.getFullPathName()}, false);
            } break;
            case 14: {
                auto current = juce::String(m.lanes[(size_t)m.selectedLane].name);
                auto* te = new juce::AlertWindow("Rename lane", "Lane name", juce::MessageBoxIconType::NoIcon);
                te->addTextEditor("n", current);
                te->addButton("OK", 1); te->addButton("Cancel", 0);
                te->enterModalState(true, juce::ModalCallbackFunction::create([this, te](int r) {
                    if (r == 1) {
                        auto s = te->getTextEditorContents("n");
                        if (s.isNotEmpty()) proc.machine().renameLane(proc.machine().selectedLane, s.toStdString());
                    }
                    delete te;
                }), false);
            } break;
            case 20: proc.setValue(G_KEY, (float)((m.keyPc + 1) % 12)); break;
            case 21: proc.setValue(G_SCALE, (float)((m.scale + 1) % 5)); break;
            case 22: proc.setValue(G_SNAPMODE, m.snapToRoot ? 0.f : 1.f); break;
        }
        proc.markDirty();
        return;
    }
}

void CrumbsEditor::mouseDrag(const juce::MouseEvent& ev) {
    if (dragKnob >= 0 && dragKnob < (int)knobs.size()) {
        auto* prm = proc.param(knobs[(size_t)dragKnob].param);
        if (!prm) return;
        float d = (dragStartY - ev.position.y) / 140.f;
        prm->setValueNotifyingHost(juce::jlimit(0.f, 1.f, dragStartVal + d));
        proc.markDirty();
        return;
    }
    if (dragSlice >= 0) {
        auto& m = proc.machine();
        int N = juce::jmax(1, m.arrangedSamples);
        float dx = ev.position.x - lastMouse.x + (ev.position.x - (timeline.getX() + timeline.getWidth() * dragStartDest / (float)N
                    + /* keep using absolute */ 0));
        // map x to samples
        int dest = (int)std::lround((ev.position.x - timeline.getX()) / juce::jmax(1.f, timeline.getWidth()) * N);
        // keep relative grab: use start + delta from mouseDown x... store nothing else; use dest at cursor
        dest = juce::jmax(0, dest);
        if (ev.mods.isAltDown() || ev.mods.isCommandDown()) {
            // extend: if dragging past the end, grow
        }
        m.setEventDestNoUndo(dragSlice, dest);
        proc.markDirty();
    }
}

void CrumbsEditor::mouseUp(const juce::MouseEvent&) {
    if (gesture && dragKnob >= 0 && dragKnob < (int)knobs.size()) {
        if (auto* prm = proc.param(knobs[(size_t)dragKnob].param)) prm->endChangeGesture();
    }
    if (dragSlice >= 0) { proc.markDirty(); proc.rebuildNow(); }
    dragSlice = dragKnob = -1; gesture = false;
}

void CrumbsEditor::mouseDoubleClick(const juce::MouseEvent& ev) {
    auto h = hitTest(ev.position);
    if (h.kind == Hit::Slice) {
        proc.machine().deleteEvent(h.index);
        proc.markDirty(); proc.rebuildNow();
    }
}

bool CrumbsEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        auto x = juce::File(f);
        auto ext = x.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac") return true;
    }
    return false;
}
void CrumbsEditor::filesDropped(const juce::StringArray& files, int, int) {
    for (auto& s : files) {
        juce::File f(s);
        juce::String err;
        if (proc.loadFile(f, err)) { lastError.clear(); toast = "Loaded " + f.getFileName(); toastUntil = juce::Time::getMillisecondCounter() + 1800; repaint(); return; }
        lastError = err;
        toast = err; toastUntil = juce::Time::getMillisecondCounter() + 2500;
    }
}
