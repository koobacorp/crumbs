#pragma once
#include "PluginProcessor.h"

class CrumbsEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CrumbsEditor(CrumbsProcessor&);
    ~CrumbsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void layout();
    void dragKnob(int param, float startVal, float dy, float sensitivity = 200.f);
    CrumbsProcessor& proc;
    // Layout rects
    juce::Rectangle<float> topBar, hintRow, modeWhole, modeSplit, btnAB, latBox, inMeter, outMeter, ceilLight;
    juce::Rectangle<float> workArea, orbArea, knobArea, splitRow, mixRow, busRow;
    std::vector<juce::Rectangle<float>> orbs, partKnobs, wholeKnobs, wholePresets, busChips, busKnobs, crunchTypes, busCrunchTypes;
    juce::Rectangle<float> mixBox, outBox, ceilBox, lookBox, lowSplitBox, highSplitBox;
    juce::Rectangle<float> listenBtn, muteBtn;
    // Drag state
    int dragParam = -1;
    float dragStart = 0, dragY0 = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrumbsEditor)
};
