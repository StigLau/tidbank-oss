#pragma once
#include <JuceHeader.h>
#include "Reel.h"

// Reels v0 (d26, d27): a reel-only looper plugin. No echo inside: chain Reels -> TidBank on one track in Live.
// Output = Input level x input + Reel level x reel. Layers lock to the host bar grid (4/4) and are not saved with the project
// (plan Q8 (a)); the parameters are. Zero latency. Allocation only in prepareToPlay (the reel pool, kPoolSeconds of stereo).
class ReelsProcessor : public juce::AudioProcessor {
public:
    // Host order. Rec is a state; Undo, Redo and Clear fire on 0 -> 1 (or from the editor buttons). Erase was taken out 2026-11-17
    // (back to the drawing board after a listening test), so new parameters go at the end.
    // Nudge: -1..+1, a hand on the reel (0x..2x); the editor's strip springs back to 0 on release.
    enum Param { Rec, Undo, Redo, Clear, Bars, ReelLevel, InputLevel, Nudge, NumParams };
    static const char* paramId(int p);
    static constexpr double kPoolSeconds = 256.0;  // plan Q7: 256 s stereo float32 (98 MB at 48 kHz)
    static constexpr float kNoHostBpm = 120.0f;
    static constexpr int kBars[3] = {1, 4, 16};  // 2026-11-18: "music is 1, 4, 16" (d37); no 2 or 8

    ReelsProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { reel.reset(); }
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
    juce::AudioProcessorValueTreeState apvts;

    // Editor buttons (message thread) -> audio thread, consumed once.
    std::atomic<bool> undoRequest { false }, redoRequest { false }, clearRequest { false };
    // Audio thread -> editor.
    struct Status {
        std::atomic<int> layers { 0 }, heard { 0 }, tempoMuted { 0 };
        std::atomic<bool> recording { false }, full { false }, recBlocked { false }, running { false }, hostTransport { false }, hostTempo { false };
        std::atomic<float> bpm { kNoHostBpm }, poolUsed { 0.0f }, speed { 1.0f }, offsetMs { 0.0f };
    } status;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float value(int p) const { const auto& prm = param(p); return prm.convertFrom0to1(prm.getValue()); }

    std::array<juce::RangedAudioParameter*, NumParams> params {};
    tidbank::Reel reel;
    juce::SmoothedValue<float> reelS, inS;
    bool undoPrev = false, redoPrev = false, clearPrev = false;
    int pulse[3] = {0, 0, 0};  // a request from the editor holds its trigger high for one block, then low
};
