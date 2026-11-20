#pragma once
#include "ReelsProcessor.h"

// Reels v0 editor: plain JUCE controls in one fixed window. Transport row (Rec, Undo, Redo, Clear), setup row (Bars, levels),
// and a status block: layers, recording, pool, and every state that silences the reel in words (full, tempo, stopped, no host).
class ReelsEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ReelsEditor(ReelsProcessor&);
    ~ReelsEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshStatus();  // the timer calls it; tests too

    juce::String statusText() const { return status.getText(); }
    juce::String warningText() const { return warning.getText(); }

private:
    void timerCallback() override { refreshStatus(); }
    ReelsProcessor& proc;
    juce::TextButton rec { "REC" }, undo { "UNDO" }, redo { "REDO" }, clear { "CLEAR" };
    juce::ComboBox bars;
    juce::Slider reelLevel, inputLevel, nudge;  // nudge: drag from anywhere, springs back to 0 on release
    juce::Label barsL, reelL, inputL, nudgeL, status, warning;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> barsA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reelA, inputA, nudgeA;
};
