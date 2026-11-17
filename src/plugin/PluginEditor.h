#pragma once
#include "PluginProcessor.h"

class CrumbsEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropContainer,
                     private juce::Timer {
public:
    explicit CrumbsEditor(CrumbsProcessor&);
    ~CrumbsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void timerCallback() override;

    CrumbsProcessor& proc;
    // Used by tests to know what was last painted / last drop error
    juce::String lastError;
    bool showingEmpty() const { return !proc.hasLoop(); }

private:
    struct Hit {
        enum Kind { None, Knob, LaneChip, Slice, Bus, Button, Palette, Mode, CrunchType } kind = None;
        int index = 0, extra = 0;
    };
    Hit hitTest(juce::Point<float>) const;
    void layout();
    juce::Rectangle<float> timeline, hintRow, laneBar, orbArea, knobRow, fineRow, busRow, busLabel, topBar;
    juce::Rectangle<float> btnAB, btnUndo, btnRedo, btnPlay, btnRebuild, btnAdd, btnSplit, btnMerge;
    juce::Rectangle<float> btnSaveA, btnSaveM, btnDragA, btnDragM, btnKeep, btnRename;
    juce::Rectangle<float> keyBox, scaleBox, snapBox, modeLoop, modeSamp;
    std::vector<juce::Rectangle<float>> laneChips, busBtns, palSwatches, crunchTypes;
    struct KnobInfo { juce::String label; int param; juce::Rectangle<float> r; bool rebuild = false; };
    std::vector<KnobInfo> knobs;
    int dragSlice = -1, dragKnob = -1;
    float dragStartVal = 0, dragStartY = 0;
    int dragStartDest = 0;
    bool gesture = false;
    juce::String toast;
    uint32_t toastUntil = 0;
    juce::Point<float> lastMouse;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrumbsEditor)
};
