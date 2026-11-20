#pragma once
#include "FiltaerProcessor.h"

// Filtær v0 editor: plain JUCE controls in one fixed window. Row 1: Cutoff, Resonance 1, Resonance 2, Spacing (knobs);
// row 2: Mode, Drive, Mix; a status line with both cutoffs in use and which filters self-oscillate.
class FiltaerEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit FiltaerEditor(FiltaerProcessor&);
    ~FiltaerEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshStatus();  // the timer calls it; tests too
    juce::String statusText() const { return status.getText(); }

private:
    void timerCallback() override { refreshStatus(); }
    FiltaerProcessor& proc;
    static constexpr int kKnobs = 6;  // Cutoff, Res 1, Res 2, Spacing, Drive, Mix
    juce::Slider knob[kKnobs];
    juce::Label knobL[kKnobs], modeL, status;
    juce::ComboBox mode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobA[kKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeA;
};
