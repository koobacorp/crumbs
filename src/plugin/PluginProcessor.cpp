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
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout CrumbsProcessor::layout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_mode", "Mode", {"Whole", "Split"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb("g_ab", "A/B dry", false)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_mix", "Mix", {0, 1, 0.01f}, 1)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_output", "Output", {-24, 12, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("g_ceiling", "Ceiling", {-3, 0, 0.01f}, -0.1f)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb("g_lookahead", "Lookahead safety", false)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("w_preset", "Whole starting point",
        {"Clean", "Tight Punch", "Fat Body", "Lo-Fi Crunch", "Wide Smash"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_punch", "Punch", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_body", "Body", {-1, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_snap", "Snap", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_crunch", "Crunch", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("w_crtype", "Crunch type", {"Tape", "Tube", "Clip", "Fuzz"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_tone", "Tone", {-1, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_glue", "Glue", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_room", "Room", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("w_width", "Width", {0, 2, 0.01f}, 1)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("g_bus", "Drum bus",
        {"Clean", "Tape Warm", "Blown Speaker", "Smashed Room", "Bedroom Crunch"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("b_punch", "Bus punch", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("b_crunch", "Bus crunch", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pc("b_crtype", "Bus crunch type", {"Tape", "Tube", "Clip", "Fuzz"}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("b_squash", "Bus squash", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("b_pump", "Bus pump", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("b_room", "Bus room", {0, 1, 0.01f}, 0)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("s_low", "Low split", {60, 400, 0.1f}, 170)));
    p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf("s_high", "High split", {2000, 12000, 1.f}, 5500)));
    const char* pn[] = {"Kick", "Snare", "Hats", "Perc"};
    const char* keys[] = {"level", "punch", "body", "decay", "crunch", "pan", "width", "mute", "listen", "sens"};
    for (int i = 0; i < kParts; ++i) {
        auto n = [&](const char* t) { return juce::String(pn[i]) + " " + t; };
        auto id = [&](const char* t) { return juce::String("p") + juce::String(i) + "_" + t; };
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("level"), n("Level"), {0, 2, 0.01f}, 1)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("punch"), n("Punch"), {0, 1, 0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("body"), n("Body"), {-1, 1, 0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("decay"), n("Decay"), {0, 1, 0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("crunch"), n("Crunch"), {0, 1, 0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("pan"), n("Pan"), {-1, 1, 0.01f}, 0)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("width"), n("Width"), {0, 2, 0.01f}, 1)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb(id("mute"), n("Mute"), false)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pb(id("listen"), n("Listen"), false)));
        p.push_back(std::unique_ptr<juce::RangedAudioParameter>(pf(id("sens"), n("Sensitivity"), {0, 1, 0.01f}, 0.5f)));
        (void)keys;
    }
    jassert((int)p.size() == kNumParams);
    return { p.begin(), p.end() };
}

CrumbsProcessor::CrumbsProcessor()
    : AudioProcessor(buses()),
      apvts(*this, nullptr, "PARAMS", layout()) {
    juce::StringArray ids {
        "g_mode","g_ab","g_mix","g_output","g_ceiling","g_lookahead",
        "w_preset","w_punch","w_body","w_snap","w_crunch","w_crtype","w_tone","w_glue","w_room","w_width",
        "g_bus","b_punch","b_crunch","b_crtype","b_squash","b_pump","b_room",
        "s_low","s_high"
    };
    const char* pk[] = {"level","punch","body","decay","crunch","pan","width","mute","listen","sens"};
    for (int i = 0; i < kParts; ++i)
        for (auto* k : pk) ids.add("p" + juce::String(i) + "_" + k);
    jassert(ids.size() == kNumParams);
    for (int i = 0; i < kNumParams; ++i) {
        params[(size_t)i] = apvts.getParameter(ids[i]);
        jassert(params[(size_t)i] != nullptr);
        raw[(size_t)i] = apvts.getRawParameterValue(ids[i]);
    }
}

void CrumbsProcessor::setValue(int i, float realValue) {
    if (auto* prm = param(i)) {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(realValue));
        prm->endChangeGesture();
    }
}

void CrumbsProcessor::applyBusPreset(int p) {
    Params tmp; tmp.applyBusPreset(p);
    setValue(B_BUS, (float)tmp.bus);
    setValue(B_PUNCH, tmp.bPunch);
    setValue(B_CRUNCH, tmp.bCrunch);
    setValue(B_CRTYPE, (float)tmp.bCrunchType);
    setValue(B_SQUASH, tmp.bSquash);
    setValue(B_PUMP, tmp.bPump);
    setValue(B_ROOM, tmp.bRoom);
    lastBus = tmp.bus;
}

void CrumbsProcessor::applyWholePreset(int p) {
    Params tmp; tmp.applyWholePreset(p);
    setValue(W_PRESET, (float)tmp.wholePreset);
    setValue(W_PUNCH, tmp.wPunch);
    setValue(W_BODY, tmp.wBody);
    setValue(W_SNAP, tmp.wSnap);
    setValue(W_CRUNCH, tmp.wCrunch);
    setValue(W_CRTYPE, (float)tmp.wCrunchType);
    setValue(W_TONE, tmp.wTone);
    setValue(W_GLUE, tmp.wGlue);
    setValue(W_ROOM, tmp.wRoom);
    setValue(W_WIDTH, tmp.wWidth);
    lastWhole = tmp.wholePreset;
}

cr::Params CrumbsProcessor::readParams() {
    Params o;
    o.mode = (int)std::lround(value(G_MODE));
    o.ab = value(G_AB) >= 0.5f;
    o.mix = value(G_MIX);
    o.outputDb = value(G_OUTPUT);
    o.ceilingDb = value(G_CEILING);
    o.lookahead = value(G_LOOKAHEAD) >= 0.5f;
    o.wholePreset = (int)std::lround(value(W_PRESET));
    o.wPunch = value(W_PUNCH); o.wBody = value(W_BODY); o.wSnap = value(W_SNAP);
    o.wCrunch = value(W_CRUNCH); o.wCrunchType = (int)std::lround(value(W_CRTYPE));
    o.wTone = value(W_TONE); o.wGlue = value(W_GLUE); o.wRoom = value(W_ROOM); o.wWidth = value(W_WIDTH);
    o.bus = (int)std::lround(value(B_BUS));
    o.bPunch = value(B_PUNCH); o.bCrunch = value(B_CRUNCH);
    o.bCrunchType = (int)std::lround(value(B_CRTYPE));
    o.bSquash = value(B_SQUASH); o.bPump = value(B_PUMP); o.bRoom = value(B_ROOM);
    o.lowSplitHz = value(S_LOW); o.highSplitHz = value(S_HIGH);
    for (int i = 0; i < kParts; ++i) {
        auto& q = o.part[i];
        q.level = value(pp(i, P_LEVEL));
        q.punch = value(pp(i, P_PUNCH));
        q.body  = value(pp(i, P_BODY));
        q.decay = value(pp(i, P_DECAY));
        q.crunch= value(pp(i, P_CRUNCH));
        q.pan   = value(pp(i, P_PAN));
        q.width = value(pp(i, P_WIDTH));
        q.mute  = value(pp(i, P_MUTE)) >= 0.5f;
        q.listen= value(pp(i, P_LISTEN)) >= 0.5f;
        q.sens  = value(pp(i, P_SENS));
    }
    return o;
}

void CrumbsProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sr = sampleRate; block = samplesPerBlock;
    eng.prepare(sampleRate, samplesPerBlock);
    eng.setParams(readParams());
    lastBus = (int)std::lround(value(B_BUS));
    lastWhole = (int)std::lround(value(W_PRESET));
    // Host-bypass delay line matches lookahead so PDC stays honest.
    delayN = std::max(1, (int)std::lround(0.0015 * sampleRate));
    delayL.assign((size_t)delayN, 0); delayR.assign((size_t)delayN, 0); delayW = 0;
    monoR.assign((size_t)std::max(samplesPerBlock, 16), 0);
    syncLatency();
}

void CrumbsProcessor::reset() {
    eng.reset();
    std::fill(delayL.begin(), delayL.end(), 0.f);
    std::fill(delayR.begin(), delayR.end(), 0.f);
    delayW = 0;
}

bool CrumbsProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto in = l.getMainInputChannelSet();
    auto out = l.getMainOutputChannelSet();
    return (in == out) && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void CrumbsProcessor::syncLatency() {
    bool la = value(G_LOOKAHEAD) >= 0.5f;
    int lat = la ? delayN : 0;
    if (lat != lastLatency) {
        lastLatency = lat;
        setLatencySamples(lat);
    }
}

void CrumbsProcessor::processDelayOnly(juce::AudioBuffer<float>& buf) {
    const int n = buf.getNumSamples();
    const int nCh = buf.getNumChannels();
    if (nCh <= 0 || delayN <= 0) return;
    float* L = buf.getWritePointer(0);
    float* R = nCh > 1 ? buf.getWritePointer(1) : nullptr;
    for (int i = 0; i < n; ++i) {
        float inL = L[i], inR = R ? R[i] : inL;
        float oL = delayL[(size_t)delayW], oR = delayR[(size_t)delayW];
        delayL[(size_t)delayW] = inL; delayR[(size_t)delayW] = inR;
        if (++delayW >= delayN) delayW = 0;
        L[i] = oL; if (R) R[i] = oR;
    }
}

void CrumbsProcessor::processBlockBypassed(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    midi.clear();
    juce::ScopedNoDenormals nd;
    syncLatency();
    if (lastLatency > 0) processDelayOnly(buf);
    // else leave the buffer as the host provided it
}

void CrumbsProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    midi.clear();
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    const int nCh = buf.getNumChannels();
    if (n <= 0 || nCh <= 0) return;

    auto pr = readParams();
    eng.setParams(pr);
    syncLatency();

    if ((int)monoR.size() < n) monoR.resize((size_t)n, 0);
    float* L = buf.getWritePointer(0);
    float* R = (nCh > 1) ? buf.getWritePointer(1) : monoR.data();
    if (nCh == 1) for (int i = 0; i < n; ++i) R[i] = L[i];
    for (int c = 2; c < nCh; ++c) buf.clear(c, 0, n);

    eng.process(L, R, n);
    inPk.store(eng.inPeak, std::memory_order_relaxed);
    outPk.store(eng.outPeak, std::memory_order_relaxed);
    hold.store(eng.ceilHold, std::memory_order_relaxed);
}

void CrumbsProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void CrumbsProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        lastBus = (int)std::lround(value(B_BUS));
        lastWhole = (int)std::lround(value(W_PRESET));
    }
}

juce::AudioProcessorEditor* CrumbsProcessor::createEditor() { return new CrumbsEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrumbsProcessor(); }
