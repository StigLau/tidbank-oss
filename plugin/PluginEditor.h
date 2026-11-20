#pragma once
#include "PluginProcessor.h"

// v0.1 editor: plain JUCE controls grouped into sections by role, in one fixed-size window. The Blender UI comes later.
// What goes where is ONE table, kSectionOf in PluginEditor.cpp: every parameter has one section, and every section says which
// engine face it belongs to (Tape, BBD or both). Switching Engine only hides and shows sections; nothing moves or resizes.
// A section binds to parameters by id only, never to another section, so it can move to a tab or window later.
// The host parameter list does not change (d16: one plugin, d20: parameter order), only what this window shows.
class TidBankEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    enum Section { Header, Performance, TimeSync, Tape, LoopFilters, Bbd, OutputSafety, NumSections };
    enum Face { Both, TapeFace, BbdFace };

    explicit TidBankEditor(TidBankProcessor&);
    ~TidBankEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshStatus();  // tempo/clamp status (+ probe strip in probe builds), face switch; the timer calls it, the snapshot tool too

    static Section sectionOf(int param);
    static Face faceOf(Section);
    static const char* title(Section);
    static bool onFace(int param, bool bbd);                  // is this parameter's section on the Tape (false) or BBD (true) face?
    static bool shown(int param, bool bbd, bool sync);        // onFace, plus the Division/Time slot that follows Sync
    const juce::Component& controlFor(int param) const { return *cells[(size_t) param].control; }  // for tests

private:
    void timerCallback() override { refreshStatus(); }
    void showFace();                                          // hide/show sections for the selected Engine
    TidBankProcessor& proc;
    struct Cell {
        juce::Label label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    };
    std::array<Cell, TidBankProcessor::NumParams> cells;
    std::array<juce::Rectangle<int>, NumSections> sectionBounds {};
    int face = -1;                                            // 0 = Tape, 1 = BBD; -1 = not shown yet
    int syncShown = -1;                                       // Sync state the Division/Time slot shows
    juce::Label status;
#ifdef TIDBANK_DEBUG_PROBE
    juce::Label probeStrip;  // debug builds only: latest delay per head and CPU from the probe
    void refreshProbe();
#endif
};
