#include "ReelsProcessor.h"
#include "ReelsEditor.h"

namespace {
constexpr const char* kIds[] = {"rec", "undo", "redo", "clear", "bars", "reel_level", "input_level", "nudge"};
static_assert(sizeof(kIds) / sizeof(kIds[0]) == ReelsProcessor::NumParams, "one id per parameter");
juce::AudioParameterFloatAttributes fmt2() {
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 2); });
}
}

const char* ReelsProcessor::paramId(int p) { return kIds[p]; }

juce::AudioProcessorValueTreeState::ParameterLayout ReelsProcessor::layout() {
    using namespace juce;
    auto id = [](int p) { return ParameterID { kIds[p], 1 }; };
    std::vector<std::unique_ptr<RangedAudioParameter>> v;  // push order = host order = enum Param
    v.push_back(std::make_unique<AudioParameterBool>(id(Rec), "Rec", false));
    v.push_back(std::make_unique<AudioParameterBool>(id(Undo), "Undo", false));    // momentary: fires on 0 -> 1
    v.push_back(std::make_unique<AudioParameterBool>(id(Redo), "Redo", false));
    v.push_back(std::make_unique<AudioParameterBool>(id(Clear), "Clear", false));
    v.push_back(std::make_unique<AudioParameterChoice>(id(Bars), "Bars", StringArray {"1", "4", "16"}, 1));
    v.push_back(std::make_unique<AudioParameterFloat>(id(ReelLevel), "Reel level", NormalisableRange<float>(0.0f, 1.0f), 1.0f, fmt2()));
    v.push_back(std::make_unique<AudioParameterFloat>(id(InputLevel), "Input level", NormalisableRange<float>(0.0f, 1.0f), 1.0f, fmt2()));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Nudge), "Nudge", NormalisableRange<float>(-1.0f, 1.0f), 0.0f, fmt2()));
    return { v.begin(), v.end() };
}

ReelsProcessor::ReelsProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Reels", layout()) {
    for (int p = 0; p < NumParams; ++p) params[(size_t) p] = apvts.getParameter(kIds[p]);
}

bool ReelsProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void ReelsProcessor::prepareToPlay(double sr, int) {
    reel.prepare(sr, kPoolSeconds, 2);  // the only allocation: the reel is empty after a sample-rate change
    for (auto* s : {&reelS, &inS}) s->reset(sr, 0.02);
    reelS.setCurrentAndTargetValue(value(ReelLevel)); inS.setCurrentAndTargetValue(value(InputLevel));
}

void ReelsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    tidbank::ReelHost h; h.bpm = kNoHostBpm;
    bool hostTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm(); b && *b > 0.0) { h.bpm = *b; hostTempo = true; }
            if (auto q = pos->getPpqPosition()) { h.hasPpq = true; h.ppq = *q; h.playing = pos->getIsPlaying(); }
        }
    // A trigger is the parameter's rising edge or an editor request (held high for one block).
    auto edge = [&](int p, std::atomic<bool>& req, bool& prevV, int& pul) {
        if (req.exchange(false)) pul = 2;
        const bool v = value(p) > 0.5f || pul == 2;
        const bool fire = v && !prevV; prevV = v; if (pul > 0) --pul;
        return fire;
    };
    tidbank::ReelParams rp;
    rp.rec = value(Rec) > 0.5f;
    rp.bars = kBars[juce::jlimit(0, 2, (int) value(Bars))];
    rp.nudge = value(Nudge);
    // the Reel detects 0 -> 1 itself; give it a one-block pulse per edge
    rp.undo = edge(Undo, undoRequest, undoPrev, pulse[0]);
    rp.redo = edge(Redo, redoRequest, redoPrev, pulse[1]);
    rp.clear = edge(Clear, clearRequest, clearPrev, pulse[2]);
    reel.setBlock(h, rp);
    reelS.setTargetValue(value(ReelLevel)); inS.setTargetValue(value(InputLevel));

    const int n = buffer.getNumSamples(), numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0) return;
    float* L = buffer.getWritePointer(0);
    float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < n; ++i) {
        const float xl = numIn > 0 ? L[i] : 0.0f;
        const float xr = numIn > 1 ? R[i] : xl;  // mono in -> stereo out: the reel records the mono input on both sides
        const float x[2] = {xl, xr}; float y[2];
        reel.process(x, y);
        const float gi = inS.getNextValue(), gr = reelS.getNextValue();
        L[i] = gi * xl + gr * y[0];
        if (R != nullptr) R[i] = gi * xr + gr * y[1];
    }

    using P = tidbank::Reel;
    status.layers.store((int) reel.probe(P::kLayers)); status.heard.store((int) reel.probe(P::kHeard));
    status.tempoMuted.store((int) reel.probe(P::kTempoMuted));
    status.recording.store(reel.probe(P::kRecording) > 0.5); status.full.store(reel.probe(P::kFull) > 0.5);
    status.recBlocked.store(reel.probe(P::kRecBlocked) > 0.5); status.running.store(reel.probe(P::kRunning) > 0.5);
    status.hostTransport.store(h.hasPpq); status.hostTempo.store(hostTempo);
    status.bpm.store((float) h.bpm); status.poolUsed.store((float) reel.probe(P::kPoolUsed));
    status.speed.store((float) reel.probe(P::kSpeed)); status.offsetMs.store((float) (reel.probe(P::kOffset) / getSampleRate() * 1000.0));
}

juce::AudioProcessorEditor* ReelsProcessor::createEditor() { return new ReelsEditor(*this); }

void ReelsProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void ReelsProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReelsProcessor(); }
