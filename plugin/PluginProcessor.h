#pragma once
#include <JuceHeader.h>
#include "TapeEngine.h"
#include "BbdEngine.h"
#ifdef TIDBANK_DEBUG_PROBE
#include "ProbeLog.h"
#endif

// TidBank v0.1: stereo = two independent mono TapeEngines (D8), plus BBD engines (d16, d18 simple path) for every
// profile (d21), prepared up front so a profile switch never allocates. Zero reported latency: the dry path is untouched.
// MIDI: none in the plugin (d20). The host maps controllers to parameters; the parameter ORDER below is what a
// controller's banks of 8 knobs see (Ableton Move/Push): bank 1 = performance, bank 2 = setup.
class TidBankProcessor : public juce::AudioProcessor {
public:
    enum Param { Intensity, Division, Mode, Nudge, Inertia, FbLpf, FbHpf, Echo,      // bank 1
                 TimeMs, Drive, Spacing, HeadTarget, Sync, Dry, Output, Panic,       // bank 2
                 Engine, BbdProfile, BbdBright, BbdSpacing, PingPong, BbdDroop,      // bank 3
                 NumParams };
    static const char* paramId(int p);

    TidBankProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }  // VST3 validator: program 0 needs a name
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::RangedAudioParameter& param(int p) const { return *params[(size_t) p]; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> bpmInUse { 120.0f };
    std::atomic<bool> hostTempo { false }, clamped { false }, bbdClamped { false };
    // Panic: fade the echo out in 10 ms, then erase the tape (dry keeps playing). From the UI button (this flag)
    // or the automatable `panic` parameter (rising edge), so a host-mapped button or knob can fire it.
    std::atomic<bool> panicRequest { false };
#ifdef TIDBANK_DEBUG_PROBE
    const tidbank::probe::PluginProbe::LiveView& probeLive() const { return probe_.live; }  // editor's probe strip
#endif

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float value(int p) const { const auto& prm = param(p); return prm.convertFrom0to1(prm.getValue()); }

    std::array<juce::RangedAudioParameter*, NumParams> params {};
    tidbank::TapeEngine eng[2];
    static constexpr int kProfiles = 3;
    tidbank::BbdEngine bbd[kProfiles][2];
    int bbdActive = 0; float bbdGain = 1.0f, bbdStep = 1.0f / 480.0f;  // profile switch: 10 ms out, swap + reset, 10 ms in
    juce::SmoothedValue<float> dryS, echoS, outS, engS;                   // engS: 0 = tape, 1 = BBD (50 ms, d16)
    int panicLen = 480, panicLeft = 0; bool panicPrev = false;
#ifdef TIDBANK_DEBUG_PROBE
    tidbank::probe::PluginProbe probe_;  // debug probe: off unless TIDBANK_PROBE or probe.cfg
#endif
};
