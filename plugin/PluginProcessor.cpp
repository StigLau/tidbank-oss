#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
// Divisions in quarter notes, shortest first. Labels match.
constexpr float kDivQ[] = {0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f};
const juce::StringArray kDivNames {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2"};
constexpr int kDivDefault = 5;  // 1/8
constexpr float kNoHostBpm = 120.0f;
// Same order as TidBankProcessor::Param (= host order). Ids are stable: hosts key automation and state on them.
constexpr const char* kIds[] = {"intensity", "division", "mode", "nudge_cents", "inertia_ms", "fb_lpf", "fb_hpf", "echo_level",
                                "time_ms", "drive", "spacing", "head_target", "sync", "dry_level", "output", "panic",
                                "engine", "bbd_profile", "bbd_bright", "bbd_spacing", "ping_pong", "bbd_droop"};
juce::AudioParameterFloatAttributes fmt(int decimals, const char* unit = "") {
    return juce::AudioParameterFloatAttributes().withLabel(unit)
        .withStringFromValueFunction([decimals](float v, int) {  // juce::String(v, 0) means full precision, not 0 decimals
            return decimals == 0 ? juce::String(juce::roundToInt(v)) : juce::String(v, decimals); });
}
juce::NormalisableRange<float> logRange(float lo, float hi, float centre) {
    juce::NormalisableRange<float> r(lo, hi); r.setSkewForCentre(centre); return r;
}
}

const char* TidBankProcessor::paramId(int p) { return kIds[p]; }

juce::AudioProcessorValueTreeState::ParameterLayout TidBankProcessor::layout() {
    using namespace juce;
    auto id = [](int p) { return ParameterID { kIds[p], 1 }; };
    StringArray modes; for (int m = 1; m <= 12; ++m) modes.add(String(m));
    std::vector<std::unique_ptr<RangedAudioParameter>> v;  // push order = host order: keep it equal to enum Param
    // bank 1: performance
    v.push_back(std::make_unique<AudioParameterFloat>(id(Intensity), "Intensity", NormalisableRange<float>(0.0f, 1.4f), 0.45f, fmt(2)));  // 1.4: d36
    v.push_back(std::make_unique<AudioParameterChoice>(id(Division), "Division", kDivNames, kDivDefault));
    v.push_back(std::make_unique<AudioParameterChoice>(id(Mode), "Mode", modes, 3));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Nudge), "Nudge", NormalisableRange<float>(-400.0f, 400.0f, 0.1f), 0.0f, fmt(1, "ct")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Inertia), "Inertia", logRange(30.0f, 2000.0f, 300.0f), 300.0f, fmt(0, "ms")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(FbLpf), "Loop LPF", logRange(1000.0f, 12000.0f, 3500.0f), 6000.0f, fmt(0, "Hz")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(FbHpf), "Loop HPF", logRange(20.0f, 500.0f, 100.0f), 80.0f, fmt(0, "Hz")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Echo), "Echo", NormalisableRange<float>(0.0f, 1.0f), 0.5f, fmt(2)));
    // bank 2: setup
    v.push_back(std::make_unique<AudioParameterFloat>(id(TimeMs), "Time (sync off)", logRange(40.0f, 2000.0f, 300.0f), 150.0f, fmt(0, "ms")));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Drive), "Drive", NormalisableRange<float>(-12.0f, 18.0f, 0.1f), 0.0f, fmt(1, "dB")));
    v.push_back(std::make_unique<AudioParameterChoice>(id(Spacing), "Spacing", StringArray {"Even 1:2:3", "Vintage"}, 0));
    v.push_back(std::make_unique<AudioParameterChoice>(id(HeadTarget), "Target head", StringArray {"1", "2", "3"}, 0));
    v.push_back(std::make_unique<AudioParameterBool>(id(Sync), "Sync", true));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Dry), "Dry", NormalisableRange<float>(0.0f, 1.0f), 1.0f, fmt(2)));
    v.push_back(std::make_unique<AudioParameterFloat>(id(Output), "Output", NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f, fmt(1, "dB")));
    v.push_back(std::make_unique<AudioParameterBool>(id(Panic), "Panic", false));  // momentary: triggers on 0 -> 1
    // bank 3
    v.push_back(std::make_unique<AudioParameterChoice>(id(Engine), "Engine", StringArray {"Tape", "BBD"}, 0));
    // Profile switching is not automatable (d21). Generic names, no trademarks; order = tidbank::profileAt().
    v.push_back(std::make_unique<AudioParameterChoice>(id(BbdProfile), "BBD profile", StringArray {"M-8192", "D-4096", "C-8192"}, 0,
                                                       AudioParameterChoiceAttributes().withAutomatable(false)));
    v.push_back(std::make_unique<AudioParameterBool>(id(BbdBright), "BBD bright", true));
    // Matriarch-style stereo: SPACING offsets the left line (Delay 1) against the right (Delay 2, the reference);
    // PING PONG cross-couples them. BBD only; the tape gets its ping-pong with the Volante head pattern later (D8).
    v.push_back(std::make_unique<AudioParameterFloat>(id(BbdSpacing), "BBD spacing", NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, fmt(0, "%")));
    v.push_back(std::make_unique<AudioParameterBool>(id(PingPong), "Ping-pong", false));
    // d36: Authentic = the BBD output hold sets where long delays run away (d4096 Intensity 1.31 at 300 ms);
    // Compensated = runaway at Intensity 1 at every delay. A listening A/B; Authentic is the expected one.
    v.push_back(std::make_unique<AudioParameterChoice>(id(BbdDroop), "BBD hold droop", StringArray {"Authentic", "Compensated"}, 0));
    return { v.begin(), v.end() };
}

TidBankProcessor::TidBankProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TidBank", layout()) {
    for (int p = 0; p < NumParams; ++p) params[(size_t) p] = apvts.getParameter(kIds[p]);
    jassert(getParameters().size() == NumParams && getParameters()[0] == params[0] && getParameters()[NumParams - 1] == params[NumParams - 1]);
}

bool TidBankProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void TidBankProcessor::prepareToPlay(double sr, int) {
    for (auto& e : eng) e.prepare(sr, 8.0);  // allocation happens here only
    static_assert(kProfiles == 3, "profile choice list and tidbank::profileCount() must agree");
    for (int k = 0; k < kProfiles; ++k) for (auto& b : bbd[k]) b.prepare(sr, tidbank::profileAt(k));
    bbdActive = juce::jlimit(0, kProfiles - 1, (int) value(BbdProfile)); bbdGain = 1.0f; bbdStep = (float) (1.0 / (0.010 * sr));
    for (auto* s : {&dryS, &echoS, &outS}) s->reset(sr, 0.02);
    engS.reset(sr, 0.05); engS.setCurrentAndTargetValue(value(Engine));
    panicLen = std::max(1, (int) (0.010 * sr)); panicLeft = 0;
    dryS.setCurrentAndTargetValue(value(Dry)); echoS.setCurrentAndTargetValue(value(Echo));
    outS.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(Output)));
#ifdef TIDBANK_DEBUG_PROBE
    {   // channel 0 of each engine is traced (channel 1 has identical timing); tracers stay detached unless configured
        const char* names[3] = {tidbank::profileAt(0).id, tidbank::profileAt(1).id, tidbank::profileAt(2).id};
        const bool on = probe_.configure(sr, names);
        eng[0].attachProbe(on ? &probe_.tape : nullptr);
        for (int k = 0; k < kProfiles; ++k) bbd[k][0].attachProbe(on ? &probe_.bbd[k] : nullptr);
    }
#endif
}

void TidBankProcessor::reset() {  // allocation-free (may run on the audio thread)
    for (auto& e : eng) e.reset();
    for (auto& k : bbd) for (auto& b : k) b.reset();
}

void TidBankProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
#ifdef TIDBANK_DEBUG_PROBE
    tidbank::probe::BlockScope probeScope(probe_.enabled ? &probe_.hub : nullptr, buffer.getNumSamples());
#endif

    float bpm = kNoHostBpm; bool fromHost = false;  // no host tempo (Standalone): fixed 120 BPM (Tempo knob removed)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b && *b > 0.0) { bpm = (float) *b; fromHost = true; }
    bpmInUse.store(bpm); hostTempo.store(fromHost);
#ifdef TIDBANK_DEBUG_PROBE
    probe_.hub.bpm = bpm;
#endif

    tidbank::TapeParams tp;
    tp.sync = value(Sync) > 0.5f ? 1 : 0;
    tp.bpm = bpm;
    tp.division = kDivQ[juce::jlimit(0, 11, (int) value(Division))];
    tp.head_target = (int) value(HeadTarget) + 1;
    tp.spacing = (int) value(Spacing);
    tp.nudge_cents = value(Nudge);
    tp.time_ms = value(TimeMs);
    tp.inertia_ms = value(Inertia);
    tp.mode = (int) value(Mode) + 1;
    tp.intensity = value(Intensity);
    tp.fb_hpf = value(FbHpf);
    tp.fb_lpf = value(FbLpf);
    tp.drive = value(Drive);
    const bool pv = value(Panic) > 0.5f;
    if ((pv && !panicPrev) || panicRequest.exchange(false)) panicLeft = panicLen;
    panicPrev = pv;
    for (auto& e : eng) e.setParams(tp);
    clamped.store(eng[0].isClamped());

    // BBD: Intensity = feedback; delay from Sync/Division/host tempo or Time, then Nudge (a detune, as on tape: in both modes,
    // 2026-11-17 "Works on both").
    // Right line (Delay 2) = that time; left line (Delay 1) = offset by BBD spacing: free, x 2^(spacing/100), so
    // -100..+100 % = half to double; synced, spacing steps through the division list from Division (+-6 steps at the ends).
    const double nudgeF = std::exp2(-tp.nudge_cents / 1200.0), beatMs = 60000.0 / std::max(bpm, 1.0f);
    const float spacing = value(BbdSpacing);
    tidbank::BbdParams bp[2];  // [0] = left (Delay 1), [1] = right (Delay 2)
    bp[1].time_ms = (float) ((tp.sync ? tp.division * beatMs : (double) tp.time_ms) * nudgeF);
    if (tp.sync) {
        const int idx = juce::jlimit(0, 11, (int) value(Division) + (int) std::lround(spacing / 100.0f * 6.0f));
        bp[0].time_ms = (float) (kDivQ[idx] * beatMs * nudgeF);
    } else bp[0].time_ms = (float) (tp.time_ms * nudgeF * std::exp2(spacing / 100.0));
    bp[0].spaced = 1;
    for (auto& b : bp) { b.feedback = tp.intensity; b.bright = value(BbdBright) > 0.5f ? 1 : 0; b.droopComp = (int) value(BbdDroop); }
    const bool pingPong = value(PingPong) > 0.5f;
    const int wantProfile = juce::jlimit(0, kProfiles - 1, (int) value(BbdProfile));
    {
        const auto pr = tidbank::profileAt(bbdActive);  // POD copy, no allocation
        bool cl = false;
        for (auto& b : bp) cl = cl || b.time_ms < pr.delayMinMs.v || b.time_ms > (b.spaced ? pr.spacedMaxMs.v : pr.delayMaxMs.v);
        bbdClamped.store(cl);
    }
    for (int c = 0; c < 2; ++c) bbd[bbdActive][c].setParams(bp[c]);
    engS.setTargetValue(value(Engine));
    dryS.setTargetValue(value(Dry)); echoS.setTargetValue(value(Echo));
    outS.setTargetValue(juce::Decibels::decibelsToGain(value(Output)));

    const int n = buffer.getNumSamples(), numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0) return;
    float* L = buffer.getWritePointer(0);
    float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < n; ++i) {
        const float xl = numIn > 0 ? L[i] : 0.0f;
        const float xr = numIn > 1 ? R[i] : xl;  // mono in -> stereo out: both engines get the mono input
        const float d = dryS.getNextValue(), o = outS.getNextValue(), e = engS.getNextValue();
        float w = echoS.getNextValue();
        if (panicLeft > 0) {
            w *= (float) --panicLeft / (float) panicLen;  // linear fade of the echo
            if (panicLeft == 0) reset();  // erase tape and BBD lines, allocation-free
        }
        if (wantProfile != bbdActive) {  // fade the old profile out, then swap to the new one (prepared, reset) and fade in
            bbdGain -= bbdStep;
            if (bbdGain <= 0.0f) { bbdGain = 0.0f; bbdActive = wantProfile; for (int c = 0; c < 2; ++c) { bbd[bbdActive][c].reset(); bbd[bbdActive][c].setParams(bp[c]); } }
        } else bbdGain = std::min(1.0f, bbdGain + bbdStep);
        // Both engines always run (d16): the incoming one already holds echoes of the recent input.
        auto& b1 = bbd[bbdActive][0]; auto& b2 = bbd[bbdActive][1];
        const float tl = eng[0].process(xl);
        float bl, br = 0.0f;
        if (pingPong && R != nullptr) {  // Matriarch PING PONG: mono sum into Delay 1 only; D1 -> D2 -> D1
            const double o1 = b1.lastOut(), o2 = b2.lastOut();
            bl = b1.process(0.5f * (xl + xr), o2);
            br = b2.process(0.0f, o1);
        } else {
            bl = b1.process(xl);
            if (R != nullptr) br = b2.process(xr);
        }
        L[i] = o * (d * xl + w * (tl + e * (bbdGain * bl - tl)));
        if (R != nullptr) {
            const float tr = eng[1].process(xr);
            R[i] = o * (d * xr + w * (tr + e * (bbdGain * br - tr)));
        }
#ifdef TIDBANK_DEBUG_PROBE
        if (probe_.enabled) probe_.hub.advance();
#endif
    }
}

juce::AudioProcessorEditor* TidBankProcessor::createEditor() { return new TidBankEditor(*this); }

void TidBankProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void TidBankProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return;
    xml->deleteAllChildElementsWithTagName("MIDIMAP");  // states saved by the d15 build carried a learn map
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TidBankProcessor(); }
