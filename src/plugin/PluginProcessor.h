#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Machine.h"

enum : int {
    G_MODE = 0, G_KEY, G_SCALE, G_KEEPKEY, G_SNAPMODE, G_AB,
    G_BUS, G_PUNCH, G_CRUNCH, G_CRTYPE, G_SQUASH, G_PUMP, G_BROOM,
    kGlobalParams = 13,
    LP_LEVEL = 0, LP_MUTE, LP_PITCH, LP_DECAY, LP_TONE, LP_CRUNCH, LP_PAN, LP_ROOM,
    LP_SWING, LP_HUMAN, LP_GHOSTS, LP_BLEND,
    LP_KPUNCH, LP_KBOOM, LP_KTUNE, LP_KCRUNCH, LP_KBODY, LP_KSNAP, LP_KLENGTH, LP_KSHIMMER,
    kLaneParams = 20,
    kNumParams = kGlobalParams + 8 * kLaneParams
};
inline int lp(int lane, int p) { return kGlobalParams + lane * kLaneParams + p; }

class CrumbsProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    CrumbsProcessor();
    ~CrumbsProcessor() override;
    static juce::AudioProcessor::BusesProperties buses();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CRUMBS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
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

    cr::Machine& machine() { return mach; }
    const cr::Machine& machine() const { return mach; }

    bool loadFile(const juce::File& f, juce::String& error);
    bool loadWave(cr::Wave w);
    bool loadTestLoop(); // synthetic 2-bar kit, for tests
    bool saveAudio(const juce::File& f, int lane = -1);
    bool saveMidi(const juce::File& f);
    juce::File writeTempAudio(int lane = -1);
    juce::File writeTempMidi();

    void markDirty() { dirty.store(true); }
    void rebuildNow(); // message thread
    void pullParamsToMachine();
    void pushMachineToParams();

    void setPreview(bool on) { preview.store(on); if (!on) previewPpq.store(0); }
    bool isPreview() const { return preview.load(); }
    bool hasLoop() const { return mach.original.n() > 0; }

    juce::AudioFormatManager formats;

private:
    void timerCallback() override;
    cr::Machine mach;
    juce::SpinLock cacheLock;
    cr::Wave cache;
    cr::Wave laneShot[8];
    struct Voice { int lane = -1; float pos = 0; float gain = 1; bool on = false; };
    Voice voices[8] {};
    std::array<juce::RangedAudioParameter*, kNumParams> params {};
    std::array<std::atomic<float>*, kNumParams> raw {};
    std::atomic<bool> dirty { false };
    std::atomic<bool> preview { false };
    std::atomic<double> previewPpq { 0 };
    double sr = 44100;
    int block = 512;
    juce::AudioBuffer<float> tmp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrumbsProcessor)
};
