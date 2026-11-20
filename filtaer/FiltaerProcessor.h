#pragma once
#include <JuceHeader.h>
#include "Filtaer.h"

// Filtær v0 (d38): the Matriarch's two ladder filters as a stereo effect, chained before TidBank in Live
// (the Matriarch's filters also sit before its delay, MM p. 34). Zero latency, no oversampling. No allocation after prepare.
class FiltaerProcessor : public juce::AudioProcessor {
public:
    // Host order (banks of 8 on Move/Push); new parameters go at the end.
    enum Param { Cutoff, Res1, Res2, Spacing, Mode, Drive, Mix, NumParams };
    static const char* paramId(int p);

    FiltaerProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { filt.reset(); }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::RangedAudioParameter& param(int p) const { return *params[(size_t) p]; }
    float value(int p) const { const auto& prm = param(p); return prm.convertFrom0to1(prm.getValue()); }
    juce::AudioProcessorValueTreeState apvts;

    // Audio thread -> editor: the cutoffs in use after smoothing and the clamp (VCF 1 includes SPACING).
    std::atomic<float> vcf1Hz { 0.0f }, vcf2Hz { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::array<juce::RangedAudioParameter*, NumParams> params {};
    tidbank::Filtaer filt;
};
