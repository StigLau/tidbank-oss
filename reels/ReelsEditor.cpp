#include "ReelsEditor.h"
#include "BuildInfo.h"

namespace {
const juce::Colour kBg(0xff1d1f22), kPanel(0xff2a2d31), kText(0xffe6e6e6), kDim(0xff9aa0a6), kRed(0xffd93a3a), kAmber(0xffe0a030);
}

ReelsEditor::ReelsEditor(ReelsProcessor& p) : AudioProcessorEditor(p), proc(p) {
    rec.setClickingTogglesState(true); addAndMakeVisible(rec);
    rec.setColour(juce::TextButton::buttonOnColourId, kRed);
    recA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, ReelsProcessor::paramId(ReelsProcessor::Rec), rec);
    undo.onClick = [this] { proc.undoRequest.store(true); };
    redo.onClick = [this] { proc.redoRequest.store(true); };
    clear.onClick = [this] { proc.clearRequest.store(true); };
    for (auto* b : {&undo, &redo, &clear}) addAndMakeVisible(b);

    bars.addItemList({"1 bar", "4 bars", "16 bars"}, 1);
    addAndMakeVisible(bars);
    barsA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, ReelsProcessor::paramId(ReelsProcessor::Bars), bars);
    for (auto* s : {&reelLevel, &inputLevel}) {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        addAndMakeVisible(s);
    }
    reelA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, ReelsProcessor::paramId(ReelsProcessor::ReelLevel), reelLevel);
    inputA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, ReelsProcessor::paramId(ReelsProcessor::InputLevel), inputLevel);
    // Nudge: press anywhere on the strip and drag (left brakes to a stop, right spins up to 2x); release springs it back to 0 and
    // the motor slews the reel back to 1x. Only the speed returns: the reel stays off the bar grid until Clear (grid option (b)).
    nudge.setSliderStyle(juce::Slider::LinearHorizontal);
    nudge.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    nudge.setSliderSnapsToMousePosition(false);
    nudge.setDoubleClickReturnValue(true, 0.0);
    nudge.onDragEnd = [this] { nudge.setValue(0.0, juce::sendNotificationSync); };
    addAndMakeVisible(nudge);
    nudgeA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, ReelsProcessor::paramId(ReelsProcessor::Nudge), nudge);
    nudgeL.setText("Nudge", juce::dontSendNotification);
    barsL.setText("Next layer", juce::dontSendNotification);
    reelL.setText("Reel level", juce::dontSendNotification);
    inputL.setText("Input level", juce::dontSendNotification);
    for (auto* l : {&barsL, &reelL, &inputL, &nudgeL}) { l->setColour(juce::Label::textColourId, kDim); addAndMakeVisible(l); }
    status.setColour(juce::Label::textColourId, kText);
    warning.setColour(juce::Label::textColourId, kAmber);
    for (auto* l : {&status, &warning}) { l->setJustificationType(juce::Justification::centredLeft); addAndMakeVisible(l); }
    setSize(620, 376);
    refreshStatus();
    startTimerHz(15);
}

void ReelsEditor::refreshStatus() {
    auto& s = proc.status;
    const int layers = s.layers.load(), heard = s.heard.load();
    juce::String st = "Layers " + juce::String(heard) + (layers > heard ? " of " + juce::String(layers) + " (undone above)" : "")
                    + "  ·  pool " + juce::String(juce::roundToInt(100.0f * s.poolUsed.load())) + " %"
                    + "  ·  " + juce::String(s.bpm.load(), 2) + " BPM, 4/4  ·  build " + TIDBANK_BUILD;
    if (std::abs(s.speed.load() - 1.0f) > 1e-4f) st += "  ·  reel " + juce::String(s.speed.load(), 2) + "x";
    if (std::abs(s.offsetMs.load()) >= 0.05f) st += "  ·  " + juce::String(s.offsetMs.load(), 1) + " ms off the grid (Clear resets)";
    if (s.recording.load()) st = "RECORDING  ·  " + st;
    status.setText(st, juce::dontSendNotification);

    juce::StringArray w;  // everything that keeps the reel quiet, in words (a silent fallback is a failure)
    if (s.full.load()) w.add("Reel full: the last take was refused (16 layers or 256 s). Undo and record, or Clear.");
    if (s.tempoMuted.load() > 0) w.add(juce::String(s.tempoMuted.load()) + " layer(s) recorded at another tempo: muted until the tempo is back (no varispeed yet).");
    if (!s.hostTransport.load()) w.add("No host transport: free running at " + juce::String(s.bpm.load(), 0) + " BPM from start-up.");
    else if (!s.running.load() && layers > 0) w.add("Transport stopped: the reel plays when the host plays.");
    else if (!s.running.load() && proc.param(ReelsProcessor::Rec).getValue() > 0.5f) w.add("Rec armed: recording starts when the host plays.");
    warning.setText(w.joinIntoString("  "), juce::dontSendNotification);

    // Rec refused or cut (full, Undo during a take, Clear): release the Rec button so its light matches what happens.
    auto& recP = proc.param(ReelsProcessor::Rec);
    if (s.recBlocked.load() && recP.getValue() > 0.5f) { recP.beginChangeGesture(); recP.setValueNotifyingHost(0.0f); recP.endChangeGesture(); }
    rec.setButtonText(s.recording.load() ? "REC  ●" : "REC");
}

void ReelsEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kPanel);
    g.fillRoundedRectangle(getLocalBounds().reduced(10).withTrimmedTop(40).toFloat(), 8.0f);
    g.setColour(kText); g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Reels", 18, 8, 200, 30, juce::Justification::centredLeft);
    g.setColour(kDim); g.setFont(juce::FontOptions(13.0f));
    g.drawText("v0: layers on the bar grid; nudge by hand; no print, no varispeed yet", 120, 8, 480, 30, juce::Justification::centredRight);
}

void ReelsEditor::resized() {
    auto r = getLocalBounds().reduced(24).withTrimmedTop(40);
    auto row = r.removeFromTop(56);
    const int bw = (row.getWidth() - 3 * 10) / 4;
    for (auto* b : {&rec, &undo, &redo, &clear}) { b->setBounds(row.removeFromLeft(bw)); row.removeFromLeft(10); }
    r.removeFromTop(14);
    auto line = [&](juce::Label& l, juce::Component& c) { auto a = r.removeFromTop(28); l.setBounds(a.removeFromLeft(100)); c.setBounds(a); r.removeFromTop(6); };
    { auto a = r.removeFromTop(28); barsL.setBounds(a.removeFromLeft(100)); bars.setBounds(a.removeFromLeft(140)); r.removeFromTop(6); }
    line(reelL, reelLevel);
    line(inputL, inputLevel);
    line(nudgeL, nudge);
    r.removeFromTop(4);
    status.setBounds(r.removeFromTop(22));
    warning.setBounds(r.removeFromTop(40));  // two lines: several warnings can apply at once
}
