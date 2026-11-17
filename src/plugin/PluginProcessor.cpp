#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace cr;

static juce::AudioParameterFloat* pf(const juce::String& id, const juce::String& name,
                                     juce::NormalisableRange<float> r, float def) {
    return new juce::AudioParameterFloat(juce::ParameterID{id, 1}, name, r, def);
}
static juce::AudioParameterBool* pb(const juce::String& id, const juce::String& name, bool def) {
    return new juce::AudioParameterBool(juce::ParameterID{id, 1}, name, def);
}
static juce::AudioParameterChoice* pc(const juce::String& id, const juce::String& name,
                                      const juce::StringArray& ch, int def) {
    return new juce::AudioParameterChoice(juce::ParameterID{id, 1}, name, ch, def);
}

juce::AudioProcessor::BusesProperties CrumbsProcessor::buses() {
    return BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout CrumbsProcessor::layout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_mode", "Mode", {"Loop", "Sampler"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_key", "Key",
        {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_scale", "Scale",
        {"Major","Minor","Chromatic","Maj Pent","Min Pent"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb("g_keepkey", "Keep in key", false)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_snapmode", "Snap", {"Nearest","Root"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb("g_ab", "A/B original", false)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_bus", "Drum bus",
        {"Clean","Tape Warm","Blown Speaker","Smashed Room","Bedroom Crunch"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_punch", "Bus punch", {0,1,0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_crunch", "Bus crunch", {0,1,0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_crtype", "Crunch type", {"Tape","Tube","Clip","Fuzz"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_squash", "Bus squash", {0,1,0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_pump", "Bus pump", {0,1,0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_broom", "Bus room", {0,1,0.01f}, 0)));
    const char* ln[] = {"Kick","Snare","Hats","Perc","Lane5","Lane6","Lane7","Lane8"};
    for (int i = 0; i < 8; ++i) {
        auto n = [&](const char* t) { return juce::String(ln[i]) + " " + t; };
        auto id = [&](const char* t) { return juce::String("l") + juce::String(i + 1) + "_" + t; };
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("level"), n("Level"), {0,2,0.01f}, 1)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb(id("mute"), n("Mute"), false)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("pitch"), n("Pitch"), {-12,12,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("decay"), n("Decay"), {0.05f,1.5f,0.01f}, 1)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("tone"), n("Tone"), {-1,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("crunch"), n("Crunch"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("pan"), n("Pan"), {-1,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("room"), n("Room"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("swing"), n("Swing"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("human"), n("Humanize"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("ghosts"), n("Ghosts"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("blend"), n("Blend"), {0,1,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("kpunch"), n("Punch"), {0,1,0.01f}, 0.5f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("kboom"), n("Boom"), {0,1,0.01f}, 0.5f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("ktune"), n("Tune"), {-12,12,0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("kcrunch"), n("Rebuild crunch"), {0,1,0.01f}, 0.2f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("kbody"), n("Body"), {0,1,0.01f}, 0.6f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("ksnap"), n("Snap"), {0,1,0.01f}, 0.5f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("klength"), n("Length"), {0,1,0.01f}, 0.5f)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("kshimmer"), n("Shimmer"), {0,1,0.01f}, 0.4f)));
    }
    jassert((int)p.size() == kNumParams);
    return { p.begin(), p.end() };
}

CrumbsProcessor::CrumbsProcessor()
    : AudioProcessor(buses()),
      apvts(*this, nullptr, "PARAMS", layout()) {
    formats.registerBasicFormats();
    for (int i = 0; i < kNumParams; ++i) {
        auto* prm = apvts.getParameter(apvts.state.getChild(i).getProperty("id").toString());
        // Safer: look up by known IDs after construction
    }
    // Map parameters in layout order
    juce::StringArray ids;
    ids.addArray({"g_mode","g_key","g_scale","g_keepkey","g_snapmode","g_ab",
                  "g_bus","g_punch","g_crunch","g_crtype","g_squash","g_pump","g_broom"});
    const char* lnk[] = {"level","mute","pitch","decay","tone","crunch","pan","room","swing","human",
                         "ghosts","blend","kpunch","kboom","ktune","kcrunch","kbody","ksnap","klength","kshimmer"};
    for (int i = 0; i < 8; ++i)
        for (auto* k : lnk) ids.add("l" + juce::String(i + 1) + "_" + k);
    jassert(ids.size() == kNumParams);
    for (int i = 0; i < kNumParams; ++i) {
        params[(size_t)i] = apvts.getParameter(ids[i]);
        jassert(params[(size_t)i] != nullptr);
        raw[(size_t)i] = apvts.getRawParameterValue(ids[i]);
    }
    startTimerHz(30);
}

CrumbsProcessor::~CrumbsProcessor() { stopTimer(); }

void CrumbsProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sr = sampleRate; block = samplesPerBlock;
    tmp.setSize(2, samplesPerBlock);
    dirty.store(true);
}

bool CrumbsProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto main = l.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo() && l.inputBuses.size() == 0;
}

void CrumbsProcessor::setValue(int i, float realValue) {
    if (auto* p = param(i)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(realValue));
        p->endChangeGesture();
    }
}

void CrumbsProcessor::pullParamsToMachine() {
    mach.playMode = value(G_MODE) >= 0.5f ? PLAY_SAMPLER : PLAY_LOOP;
    mach.keyPc = (int)std::lround(value(G_KEY));
    mach.scale = (int)std::lround(value(G_SCALE));
    mach.keepInKey = value(G_KEEPKEY) >= 0.5f;
    mach.snapToRoot = value(G_SNAPMODE) >= 0.5f;
    mach.abOriginal = value(G_AB) >= 0.5f;
    int preset = (int)std::lround(value(G_BUS));
    if (preset != mach.bus.preset) {
        mach.bus.applyPreset(preset);
        // applying a preset overwrites the knobs; push those back except when the user is on Clean/custom
        setValue(G_PUNCH, mach.bus.punch);
        setValue(G_CRUNCH, mach.bus.crunch);
        setValue(G_CRTYPE, (float)mach.bus.crunchType);
        setValue(G_SQUASH, mach.bus.squash);
        setValue(G_PUMP, mach.bus.pump);
        setValue(G_BROOM, mach.bus.room);
    } else {
        mach.bus.punch = value(G_PUNCH);
        mach.bus.crunch = value(G_CRUNCH);
        mach.bus.crunchType = (int)std::lround(value(G_CRTYPE));
        mach.bus.squash = value(G_SQUASH);
        mach.bus.pump = value(G_PUMP);
        mach.bus.room = value(G_BROOM);
        // if the user has moved knobs off the preset, keep the chosen preset id for the UI
        if (preset == BUS_CLEAN && !mach.bus.isClean()) mach.bus.preset = BUS_CLEAN; // still "custom clean"? leave
    }
    for (int i = 0; i < (int)mach.lanes.size() && i < 8; ++i) {
        auto& L = mach.lanes[(size_t)i];
        L.level = value(lp(i, LP_LEVEL));
        L.mute = value(lp(i, LP_MUTE)) >= 0.5f;
        L.pitch = value(lp(i, LP_PITCH));
        L.decay = value(lp(i, LP_DECAY));
        L.tone = value(lp(i, LP_TONE));
        L.crunch = value(lp(i, LP_CRUNCH));
        L.pan = value(lp(i, LP_PAN));
        L.room = value(lp(i, LP_ROOM));
        L.swing = value(lp(i, LP_SWING));
        L.humanize = value(lp(i, LP_HUMAN));
        L.ghosts = value(lp(i, LP_GHOSTS));
        L.blend = value(lp(i, LP_BLEND));
        L.kPunch = value(lp(i, LP_KPUNCH));
        L.kBoom = value(lp(i, LP_KBOOM));
        L.kTune = value(lp(i, LP_KTUNE));
        L.kCrunch = value(lp(i, LP_KCRUNCH));
        L.kBody = value(lp(i, LP_KBODY));
        L.kSnap = value(lp(i, LP_KSNAP));
        L.kLength = value(lp(i, LP_KLENGTH));
        L.kShimmer = value(lp(i, LP_KSHIMMER));
    }
}

void CrumbsProcessor::pushMachineToParams() {
    setValue(G_MODE, (float)mach.playMode);
    setValue(G_KEY, (float)mach.keyPc);
    setValue(G_SCALE, (float)mach.scale);
    setValue(G_KEEPKEY, mach.keepInKey ? 1.f : 0.f);
    setValue(G_SNAPMODE, mach.snapToRoot ? 1.f : 0.f);
    setValue(G_AB, mach.abOriginal ? 1.f : 0.f);
    setValue(G_BUS, (float)mach.bus.preset);
    setValue(G_PUNCH, mach.bus.punch);
    setValue(G_CRUNCH, mach.bus.crunch);
    setValue(G_CRTYPE, (float)mach.bus.crunchType);
    setValue(G_SQUASH, mach.bus.squash);
    setValue(G_PUMP, mach.bus.pump);
    setValue(G_BROOM, mach.bus.room);
    for (int i = 0; i < (int)mach.lanes.size() && i < 8; ++i) {
        auto& L = mach.lanes[(size_t)i];
        setValue(lp(i, LP_LEVEL), L.level);
        setValue(lp(i, LP_MUTE), L.mute ? 1.f : 0.f);
        setValue(lp(i, LP_PITCH), L.pitch);
        setValue(lp(i, LP_DECAY), L.decay);
        setValue(lp(i, LP_TONE), L.tone);
        setValue(lp(i, LP_CRUNCH), L.crunch);
        setValue(lp(i, LP_PAN), L.pan);
        setValue(lp(i, LP_ROOM), L.room);
        setValue(lp(i, LP_SWING), L.swing);
        setValue(lp(i, LP_HUMAN), L.humanize);
        setValue(lp(i, LP_GHOSTS), L.ghosts);
        setValue(lp(i, LP_BLEND), L.blend);
        setValue(lp(i, LP_KPUNCH), L.kPunch);
        setValue(lp(i, LP_KBOOM), L.kBoom);
        setValue(lp(i, LP_KTUNE), L.kTune);
        setValue(lp(i, LP_KCRUNCH), L.kCrunch);
        setValue(lp(i, LP_KBODY), L.kBody);
        setValue(lp(i, LP_KSNAP), L.kSnap);
        setValue(lp(i, LP_KLENGTH), L.kLength);
        setValue(lp(i, LP_KSHIMMER), L.kShimmer);
    }
}

void CrumbsProcessor::rebuildNow() {
    pullParamsToMachine();
    auto w = mach.render(true, mach.abOriginal);
    cr::Wave shots[8];
    for (int i = 0; i < (int)mach.lanes.size() && i < 8; ++i)
        shots[i] = mach.renderLane(i);
    const juce::SpinLock::ScopedLockType sl(cacheLock);
    cache = std::move(w);
    for (int i = 0; i < 8; ++i) laneShot[i] = std::move(shots[i]);
    dirty.store(false);
}

void CrumbsProcessor::timerCallback() {
    // Detect param changes cheaply
    static thread_local float last[kNumParams];
    static thread_local bool init = false;
    bool ch = dirty.load();
    if (!init) { ch = true; init = true; }
    for (int i = 0; i < kNumParams; ++i) {
        float v = value(i);
        if (v != last[i]) { last[i] = v; ch = true; }
    }
    if (ch) rebuildNow();
}

bool CrumbsProcessor::loadWave(cr::Wave w) {
    mach.loadWave(std::move(w));
    pushMachineToParams();
    dirty.store(true);
    rebuildNow();
    return mach.original.n() > 0;
}

bool CrumbsProcessor::loadTestLoop() {
    auto w = cr::Wave{};
    // 2-bar kit at 120 BPM, 44100
    int n = (int)std::lround(44100 * 60.0 * 8 / 120.0);
    w.resize(n, 44100);
    int step = (int)std::lround(44100 * 60.0 / 120.0 / 4.0);
    for (int b = 0; b < 8; ++b) {
        int q = b * step * 4;
        if (b % 2 == 0) addKick(w, q); else addSnare(w, q);
        addHat(w, q, 0.35f, 0.04f);
        addHat(w, q + step * 2, 0.28f, 0.04f);
        if (b % 4 == 3) addPerc(w, q + step * 3, 250, 0.5f);
    }
    return loadWave(std::move(w));
}

bool CrumbsProcessor::loadFile(const juce::File& f, juce::String& error) {
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
    if (!r) { error = "Could not read that file. Try WAV, AIFF or FLAC."; return false; }
    int n = (int)r->lengthInSamples;
    if (n < 32) { error = "That file is too short."; return false; }
    juce::AudioBuffer<float> buf((int)r->numChannels, n);
    r->read(&buf, 0, n, 0, true, true);
    cr::Wave w; w.resize(n, r->sampleRate);
    for (int i = 0; i < n; ++i) {
        w.L[(size_t)i] = buf.getSample(0, i);
        w.R[(size_t)i] = buf.getNumChannels() > 1 ? buf.getSample(1, i) : w.L[(size_t)i];
    }
    return loadWave(std::move(w));
}

static bool writeWavFile(const cr::Wave& w, const juce::File& f) {
    if (w.n() <= 0) return false;
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::WavAudioFormat fmt;
    auto os = std::unique_ptr<juce::FileOutputStream>(f.createOutputStream().release());
    if (!os) return false;
    juce::AudioBuffer<float> buf(2, w.n());
    for (int i = 0; i < w.n(); ++i) {
        buf.setSample(0, i, w.L[(size_t)i]);
        buf.setSample(1, i, i < (int)w.R.size() ? w.R[(size_t)i] : w.L[(size_t)i]);
    }
    std::unique_ptr<juce::AudioFormatWriter> wr(fmt.createWriterFor(os.get(), w.sr, 2, 24, {}, 0));
    if (!wr) return false;
    os.release();
    return wr->writeFromAudioSampleBuffer(buf, 0, w.n());
}

bool CrumbsProcessor::saveAudio(const juce::File& f, int lane) {
    pullParamsToMachine();
    auto w = (lane >= 0) ? mach.renderLane(lane) : mach.render(true, false);
    return writeWavFile(w, f);
}
bool CrumbsProcessor::saveMidi(const juce::File& f) {
    auto m = mach.exportMidi();
    if (m.empty()) return false;
    f.deleteFile();
    juce::FileOutputStream os(f);
    return os.openedOk() && os.write(m.data(), (size_t)m.size());
}
juce::File CrumbsProcessor::writeTempAudio(int lane) {
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile(lane >= 0 ? "CRUMBS-lane.wav" : "CRUMBS-loop.wav");
    saveAudio(f, lane); return f;
}
juce::File CrumbsProcessor::writeTempMidi() {
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CRUMBS-loop.mid");
    saveMidi(f); return f;
}

void CrumbsProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    const int nCh = buf.getNumChannels();
    for (int c = 0; c < nCh; ++c) buf.clear(c, 0, n);
    const juce::SpinLock::ScopedTryLockType sl(cacheLock);
    if (!sl.isLocked()) { midi.clear(); return; }

    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    }
    if (preview.load()) {
        playing = true;
        ppq = previewPpq.load();
        previewPpq.store(ppq + n * bpm / 60.0 / sr);
    }

    const bool sampler = value(G_MODE) >= 0.5f;
    if (sampler) {
        for (const auto md : midi) {
            auto m = md.getMessage();
            if (m.isNoteOn()) {
                int note = m.getNoteNumber();
                int lane = -1;
                for (int i = 0; i < (int)mach.lanes.size() && i < 8; ++i)
                    if (mach.lanes[(size_t)i].midiNote == note) { lane = i; break; }
                if (lane < 0 && note >= 36 && note < 36 + (int)mach.lanes.size()) lane = note - 36;
                if (lane >= 0 && lane < 8 && laneShot[lane].n() > 0) {
                    voices[lane].lane = lane; voices[lane].pos = 0; voices[lane].on = true;
                    voices[lane].gain = m.getFloatVelocity();
                }
            } else if (m.isNoteOff() || m.isAllNotesOff()) {
                if (m.isAllNotesOff()) { for (auto& v : voices) v.on = false; }
                else {
                    int note = m.getNoteNumber();
                    for (int i = 0; i < 8; ++i)
                        if (voices[i].on && i < (int)mach.lanes.size() && mach.lanes[(size_t)i].midiNote == note)
                            voices[i].on = false;
                }
            }
        }
        midi.clear();
        if (nCh < 2) return;
        auto* L = buf.getWritePointer(0);
        auto* R = buf.getWritePointer(1);
        for (int s = 0; s < n; ++s) {
            float l = 0, r = 0;
            for (auto& v : voices) if (v.on && v.lane >= 0) {
                auto& sh = laneShot[v.lane];
                if (v.pos >= sh.n()) { v.on = false; continue; }
                l += hermite(sh.L, v.pos) * v.gain;
                r += hermite(sh.R.empty() ? sh.L : sh.R, v.pos) * v.gain;
                v.pos += (float)(sh.sr / sr);
            }
            L[s] = l; R[s] = r;
        }
        return;
    }

    midi.clear();
    if (!playing || cache.n() <= 0 || nCh < 2) return;
    mach.mixAt(buf.getWritePointer(0), buf.getWritePointer(1), n, bpm, ppq, cache);
}

void CrumbsProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto t = apvts.copyState();
    t.setProperty("nLanes", (int)mach.lanes.size(), nullptr);
    t.setProperty("arranged", mach.arrangedSamples, nullptr);
    t.setProperty("beats", mach.inferredBeats, nullptr);
    t.setProperty("selLane", mach.selectedLane, nullptr);
    juce::MemoryBlock audio;
    if (mach.original.n() > 0) {
        juce::WavAudioFormat fmt;
        juce::AudioBuffer<float> buf(2, mach.original.n());
        for (int i = 0; i < mach.original.n(); ++i) {
            buf.setSample(0, i, mach.original.L[(size_t)i]);
            buf.setSample(1, i, i < (int)mach.original.R.size() ? mach.original.R[(size_t)i] : mach.original.L[(size_t)i]);
        }
        auto* mo = new juce::MemoryOutputStream(audio, false);
        std::unique_ptr<juce::AudioFormatWriter> wr(fmt.createWriterFor(mo, mach.original.sr, 2, 24, {}, 0));
        if (wr) wr->writeFromAudioSampleBuffer(buf, 0, mach.original.n());
        wr.reset(); // deletes mo
        t.setProperty("audio", audio.toBase64Encoding(), nullptr);
        t.setProperty("sr", mach.original.sr, nullptr);
    }
    juce::ValueTree evs("EVENTS");
    for (auto& e : mach.events) {
        juce::ValueTree ev("E");
        ev.setProperty("ss", e.srcStart, nullptr);
        ev.setProperty("se", e.srcEnd, nullptr);
        ev.setProperty("ln", e.lane, nullptr);
        ev.setProperty("ds", e.destStart, nullptr);
        evs.appendChild(ev, nullptr);
    }
    t.appendChild(evs, nullptr);
    juce::ValueTree lns("LANES");
    for (auto& L : mach.lanes) {
        juce::ValueTree ln("L");
        ln.setProperty("name", juce::String(L.name), nullptr);
        ln.setProperty("col", L.colour, nullptr);
        ln.setProperty("kind", L.kind, nullptr);
        ln.setProperty("midi", L.midiNote, nullptr);
        ln.setProperty("reb", L.rebuilt, nullptr);
        lns.appendChild(ln, nullptr);
    }
    t.appendChild(lns, nullptr);
    if (auto xml = t.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

void CrumbsProcessor::setStateInformation(const void* data, int size) {
    auto xml = juce::AudioProcessor::getXmlFromBinary(data, size);
    if (!xml) return;
    auto t = juce::ValueTree::fromXml(*xml);
    auto evs = t.getChildWithName("EVENTS");
    auto lns = t.getChildWithName("LANES");
    t.removeChild(evs, nullptr);
    t.removeChild(lns, nullptr);
    auto audioB64 = t.getProperty("audio").toString();
    t.removeProperty("audio", nullptr);
    apvts.replaceState(t);
    if (audioB64.isNotEmpty()) {
        juce::MemoryBlock audio; audio.fromBase64Encoding(audioB64);
        auto* mi = new juce::MemoryInputStream(audio, true);
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> r(fmt.createReaderFor(mi, true));
        if (r) {
            int n = (int)r->lengthInSamples;
            juce::AudioBuffer<float> buf((int)r->numChannels, n);
            r->read(&buf, 0, n, 0, true, true);
            cr::Wave w; w.resize(n, r->sampleRate);
            for (int i = 0; i < n; ++i) {
                w.L[(size_t)i] = buf.getSample(0, i);
                w.R[(size_t)i] = buf.getNumChannels() > 1 ? buf.getSample(1, i) : w.L[(size_t)i];
            }
            mach.original = std::move(w);
        }
    }
    if (lns.isValid()) {
        mach.lanes.clear();
        for (int i = 0; i < lns.getNumChildren(); ++i) {
            auto ln = lns.getChild(i);
            Lane L; L.name = ln.getProperty("name").toString().toStdString();
            L.colour = (int)ln.getProperty("col"); L.kind = (int)ln.getProperty("kind");
            L.midiNote = (int)ln.getProperty("midi"); L.rebuilt = (bool)ln.getProperty("reb");
            mach.lanes.push_back(L);
        }
        if (mach.lanes.empty()) mach.resetLanes();
    }
    pullParamsToMachine();
    mach.events.clear();
    if (evs.isValid()) {
        for (int i = 0; i < evs.getNumChildren(); ++i) {
            auto ev = evs.getChild(i);
            Event e; e.srcStart = ev.getProperty("ss"); e.srcEnd = ev.getProperty("se");
            e.lane = ev.getProperty("ln"); e.destStart = ev.getProperty("ds");
            mach.events.push_back(e);
        }
    } else if (mach.original.n() > 0) {
        mach.slices = sliceAndClassify(mach.original);
        for (auto& s : mach.slices) {
            Event e; e.srcStart = s.start; e.srcEnd = s.end; e.lane = s.lane; e.destStart = s.start;
            mach.events.push_back(e);
        }
    }
    mach.arrangedSamples = (int)t.getProperty("arranged", mach.original.n());
    mach.inferredBeats = (int)t.getProperty("beats", 8);
    mach.selectedLane = (int)t.getProperty("selLane", 0);
    dirty.store(true);
    rebuildNow();
}

juce::AudioProcessorEditor* CrumbsProcessor::createEditor() { return new CrumbsEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrumbsProcessor(); }
