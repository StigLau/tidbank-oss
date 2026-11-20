#include "FiltaerEditor.h"

namespace {
const juce::Colour kBg(0xff1d1f22), kPanel(0xff2a2d31), kText(0xffe6e6e6), kDim(0xff9aa0a6);
using P = FiltaerProcessor;
constexpr int kKnobParam[] = {P::Cutoff, P::Res1, P::Res2, P::Spacing, P::Drive, P::Mix};
constexpr const char* kKnobName[] = {"Cutoff", "Resonance 1", "Resonance 2", "Spacing", "Drive", "Mix"};
juce::String hz(float f) { return f < 999.5f ? juce::String(f, 0) + " Hz" : juce::String(f / 1000.0f, 2) + " kHz"; }
}

FiltaerEditor::FiltaerEditor(FiltaerProcessor& p) : AudioProcessorEditor(p), proc(p) {
    for (int i = 0; i < kKnobs; ++i) {
        knob[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
        addAndMakeVisible(knob[i]);
        knobA[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, P::paramId(kKnobParam[i]), knob[i]);
        knobL[i].setText(kKnobName[i], juce::dontSendNotification);
        knobL[i].setJustificationType(juce::Justification::centred);
        knobL[i].setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(knobL[i]);
    }
    mode.addItemList({"HP/LP Series", "LP/LP Stereo", "HP/LP Parallel"}, 1);
    addAndMakeVisible(mode);
    modeA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, P::paramId(P::Mode), mode);
    modeL.setText("Mode", juce::dontSendNotification);
    modeL.setJustificationType(juce::Justification::centred);
    modeL.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(modeL);
    status.setColour(juce::Label::textColourId, kText);
    status.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(status);
    setSize(620, 400);
    refreshStatus();
    startTimerHz(15);
}

void FiltaerEditor::refreshStatus() {
    const float k = (float) tidbank::Filtaer::kSelfOscKnob;
    const int m = juce::jlimit(0, 2, (int) proc.value(P::Mode));
    // in SERIES and PARALLEL VCF 1 is the high-pass
    if (proc.vcf2Hz.load() <= 0.0f) { status.setText("No audio yet: cutoffs show once the host runs the plugin", juce::dontSendNotification); return; }
    juce::String s = "VCF 1 " + juce::String(m == 1 ? "LP " : "HP ") + hz(proc.vcf1Hz.load()) + "  ·  VCF 2 LP " + hz(proc.vcf2Hz.load());
    juce::StringArray osc;
    if (proc.value(P::Res1) >= k) osc.add("VCF 1");
    if (proc.value(P::Res2) >= k) osc.add("VCF 2");
    if (!osc.isEmpty()) s << "  ·  self-oscillating: " << osc.joinIntoString(" + ");
    status.setText(s, juce::dontSendNotification);
}

void FiltaerEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kPanel);
    g.fillRoundedRectangle(getLocalBounds().reduced(10).withTrimmedTop(40).toFloat(), 8.0f);
    g.setColour(kText); g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(juce::String::fromUTF8("Filtær"), 18, 8, 200, 30, juce::Justification::centredLeft);
    g.setColour(kDim); g.setFont(juce::FontOptions(13.0f));
    g.drawText("v0: Matriarch filter section, ladder only", 120, 8, 480, 30, juce::Justification::centredRight);
}

void FiltaerEditor::resized() {
    auto r = getLocalBounds().reduced(24).withTrimmedTop(40);
    auto cell = [&](juce::Rectangle<int>& row, int w, juce::Label& l, juce::Component& c, int ch) {
        auto a = row.removeFromLeft(w); l.setBounds(a.removeFromTop(20)); c.setBounds(a.withSizeKeepingCentre(std::min(w, 100), ch));
    };
    auto row1 = r.removeFromTop(140); const int w = row1.getWidth() / 4;
    for (int i = 0; i < 4; ++i) cell(row1, w, knobL[i], knob[i], 110);
    r.removeFromTop(8);
    auto row2 = r.removeFromTop(140);
    { auto a = row2.removeFromLeft(w * 2); modeL.setBounds(a.removeFromTop(20)); mode.setBounds(a.withSizeKeepingCentre(180, 28)); }
    for (int i = 4; i < 6; ++i) cell(row2, w, knobL[i], knob[i], 110);
    status.setBounds(r.removeFromTop(24));
}
