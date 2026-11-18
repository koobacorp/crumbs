#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

enum : int {
    G_MODE = 0, G_AB, G_MIX, G_OUTPUT, G_CEILING, G_LOOKAHEAD,
    W_PRESET, W_PUNCH, W_BODY, W_SNAP, W_CRUNCH, W_CRTYPE, W_TONE, W_GLUE, W_ROOM, W_WIDTH,
    B_BUS, B_PUNCH, B_CRUNCH, B_CRTYPE, B_SQUASH, B_PUMP, B_ROOM,
    S_LOW, S_HIGH,
    kGlobalParams = 25,
    P_LEVEL = 0, P_PUNCH, P_BODY, P_DECAY, P_CRUNCH, P_PAN, P_WIDTH, P_MUTE, P_LISTEN, P_SENS,
    kPartParams = 10,
    kNumParams = kGlobalParams + cr::kParts * kPartParams
};
inline int pp(int part, int p) { return kGlobalParams + part * kPartParams + p; }

class CrumbsProcessor : public juce::AudioProcessor {
public:
    CrumbsProcessor();
    ~CrumbsProcessor() override = default;

    static juce::AudioProcessor::BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CRUMBS"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "CRUMBS"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param(int i) const { return params[(size_t)i]; }
    float value(int i) const { return raw[(size_t)i]->load(std::memory_order_relaxed); }
    void setValue(int i, float realValue);

    cr::Params readParams();
    int reportedLatency() const { return getLatencySamples(); }
    float inPeak() const { return inPk.load(std::memory_order_relaxed); }
    float outPeak() const { return outPk.load(std::memory_order_relaxed); }
    float ceilingHold() const { return hold.load(std::memory_order_relaxed); }
    int selectedPart() const { return selPart.load(std::memory_order_relaxed); }
    void setSelectedPart(int i) { selPart.store(juce::jlimit(0, cr::kParts - 1, i)); }

    // Applies a bus or whole starting-point and pushes the knobs back to the host.
    void applyBusPreset(int p);
    void applyWholePreset(int p);

private:
    void syncLatency();
    void processDelayOnly(juce::AudioBuffer<float>&);
    cr::Engine eng;
    std::array<juce::RangedAudioParameter*, kNumParams> params {};
    std::array<std::atomic<float>*, kNumParams> raw {};
    std::atomic<float> inPk { 0 }, outPk { 0 }, hold { 0 };
    std::atomic<int> selPart { 0 };
    int lastBus = 0, lastWhole = 0;
    int lastLatency = 0;
    double sr = 44100;
    int block = 512;
    std::vector<float> delayL, delayR, monoR;
    int delayN = 0, delayW = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrumbsProcessor)
};
