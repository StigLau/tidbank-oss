#include "FiltaerProcessor.h"
#include "FiltaerEditor.h"

namespace {
constexpr const char* kIds[] = {"cutoff", "res1", "res2", "spacing", "mode", "drive", "mix"};
static_assert(sizeof(kIds) / sizeof(kIds[0]) == FiltaerProcessor::NumParams, "one id per parameter");
juce::AudioParameterFloatAttributes fmt(int decimals, const char* unit = "") {
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction([=](float v, int) { return juce::String(v, decimals) + unit; });
}
}

const char* FiltaerProcessor::paramId(int p) { return kIds[p]; }

juce::AudioProcessorValueTreeState::ParameterLayout FiltaerProcessor::layout() {
    using namespace juce;
    auto id = [](int p) { return ParameterID { kIds[p], 1 }; };
    // Cutoff: exponential over the faceplate's 20 Hz - 20 kHz (the measured law is exponential; MM p. 21 end points)
    NormalisableRange<float> hz(20.0f, 20000.0f,
        [](float a, float b, float x) { return a * std::pow(b / a, x); },
        [](float a, float b, float v) { return std::log(v / a) / std::log(b / a); });
    std::vector<std::unique_ptr<RangedAudioParameter>> v;  // push order = host order = enum Param
    v.push_back(std::make_unique<AudioParameterFloat>(id(Cutoff), "Cutoff", hz, 20000.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction([](float x, int) { return x < 999.5f ? String(x, 0) + " Hz" : String(x / 1000.0f, 2) + " kHz"; })));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Res1), "Resonance 1", NormalisableRange<float>(0.0f, 1.0f), 0.0f, fmt(2)));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Res2), "Resonance 2", NormalisableRange<float>(0.0f, 1.0f), 0.0f, fmt(2)));
    const float sp = (float) tidbank::Filtaer::kSpacingMaxOct;
    v.push_back(std::make_unique<AudioParameterFloat>(id(Spacing), "Spacing", NormalisableRange<float>(-sp, sp), 0.0f, fmt(2, " oct")));
    v.push_back(std::make_unique<AudioParameterChoice>(id(Mode), "Mode", StringArray {"HP/LP Series", "LP/LP Stereo", "HP/LP Parallel"}, 1));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Drive), "Drive", NormalisableRange<float>(0.0f, 24.0f), 0.0f, fmt(1, " dB")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Mix), "Mix", NormalisableRange<float>(0.0f, 1.0f), 1.0f, fmt(2)));
    return { v.begin(), v.end() };
}

FiltaerProcessor::FiltaerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Filtaer", layout()) {
    for (int p = 0; p < NumParams; ++p) params[(size_t) p] = apvts.getParameter(kIds[p]);
}

bool FiltaerProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void FiltaerProcessor::prepareToPlay(double sr, int) { filt.prepare(sr); }

void FiltaerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    tidbank::FiltaerParams fp;
    fp.cutoffHz = value(Cutoff); fp.res1 = value(Res1); fp.res2 = value(Res2); fp.spacingOct = value(Spacing);
    fp.mode = juce::jlimit(0, 2, (int) value(Mode)); fp.driveDb = value(Drive); fp.mix = value(Mix);
    filt.setParams(fp);

    const int n = buffer.getNumSamples(), numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0) return;
    float* L = buffer.getWritePointer(0);
    float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < n; ++i) {
        const float xl = numIn > 0 ? L[i] : 0.0f;
        const float xr = numIn > 1 ? R[i] : xl;  // mono in: the same signal on both sides
        const float x[2] = {xl, xr}; float y[2];
        filt.process(x, y);
        L[i] = R != nullptr ? y[0] : 0.5f * (y[0] + y[1]);  // mono out: both filters summed
        if (R != nullptr) R[i] = y[1];
    }
    vcf1Hz.store((float) filt.cutoff1Hz()); vcf2Hz.store((float) filt.cutoff2Hz());
}

juce::AudioProcessorEditor* FiltaerProcessor::createEditor() { return new FiltaerEditor(*this); }

void FiltaerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void FiltaerProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FiltaerProcessor(); }
