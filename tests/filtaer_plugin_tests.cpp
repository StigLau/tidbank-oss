// Filtær plugin checks without a host (d38): parameter order, zero latency, mix 0 bit-exact, defaults nearly
// transparent, the parameters reach the filter (self-oscillation pitch, SPACING), mono in, state round trip, editor status and
// geometry. Optional: filtaer_plugin_tests out.png writes an offscreen snapshot of the editor.
#include <JuceHeader.h>
#include "FiltaerProcessor.h"
#include "FiltaerEditor.h"
#include <cstdio>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
using P = FiltaerProcessor;
static void set(P& p, int prm, float plain) { auto& q = p.param(prm); q.setValueNotifyingHost(q.convertTo0to1(plain)); }
static float noise(int64_t n) { uint32_t s = (uint32_t) (n * 2654435761u + 7u); s ^= s >> 15; s *= 2246822519u; s ^= s >> 13; return (float) ((s & 0xFFFF) / 65536.0 - 0.5) * 0.4f; }

// Run blocks of 512 at 48 kHz; input(i, ch); returns the output channels.
template <class F> static std::vector<std::vector<float>> run(P& p, int blocks, F in, int chIn = 2) {
    const int N = 512; juce::AudioBuffer<float> b(2, N); juce::MidiBuffer m; std::vector<std::vector<float>> out(2);
    for (int j = 0; j < blocks; ++j) {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) b.setSample(c, i, c < chIn ? in((int64_t) j * N + i, c) : 0.0f);
        p.processBlock(b, m);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) out[(size_t) c].push_back(b.getSample(c, i));
    }
    return out;
}
static double freq(const std::vector<float>& x, size_t from) {  // upward zero crossings, interpolated
    double first = -1, last = -1; int n = 0;
    for (size_t i = from + 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { const double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]); if (first < 0) first = t; else ++n; last = t; }
    return n > 0 ? n * 48000.0 / (last - first) : 0.0;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    // 1. host order and ids, latency
    {
        P p; const char* want[] = {"cutoff", "res1", "res2", "spacing", "mode", "drive", "mix"};
        CHECK(p.getParameters().size() == P::NumParams, "%d parameters, want %d", p.getParameters().size(), (int) P::NumParams);
        for (int i = 0; i < P::NumParams; ++i) {
            auto* q = dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[i]);
            CHECK(q && q->getParameterID() == want[i], "slot %d is %s, want %s", i, q ? q->getParameterID().toRawUTF8() : "?", want[i]);
        }
        CHECK(p.getLatencySamples() == 0, "latency %d", p.getLatencySamples());
        CHECK(p.param(P::Mode).getCurrentValueAsText() == "LP/LP Stereo", "default mode %s", p.param(P::Mode).getCurrentValueAsText().toRawUTF8());
    }
    // 2. mix 0: output == input, bit-exact, even with the filter self-oscillating and driven
    {
        P p; p.prepareToPlay(48000, 512);
        set(p, P::Mix, 0); set(p, P::Res1, 1); set(p, P::Res2, 1); set(p, P::Drive, 24); set(p, P::Cutoff, 300);
        auto in = [](int64_t i, int c) { return noise(i + c * 99); };
        auto o = run(p, 40, in); int bad = 0;
        for (size_t i = 0; i < o[0].size(); ++i) bad += o[0][i] != in((int64_t) i, 0) || o[1][i] != in((int64_t) i, 1);
        CHECK(bad == 0, "mix 0: %d samples differ from the input", bad);
    }
    // 3. defaults (20 kHz, resonance 0, mix 1) are nearly transparent below 5 kHz: a 1 kHz sine within 0.05 dB (the ladder's input tanh: tanh(0.1) = -0.03 dB)
    {
        P p; p.prepareToPlay(48000, 512);
        auto o = run(p, 100, [](int64_t i, int) { return (float) (0.1 * std::sin(2 * M_PI * 1000.0 * (double) i / 48000.0)); });
        double re = 0, im = 0; const size_t n0 = 24000, n1 = o[0].size();  // single-bin DFT over whole periods (48 samples)
        for (size_t i = n0; i < n1; ++i) { const double w = 2 * M_PI * 1000.0 * (double) i / 48000.0; re += o[0][i] * std::cos(w); im += o[0][i] * std::sin(w); }
        const double a = 2 * std::hypot(re, im) / (double) (n1 - n0);
        std::printf("defaults: 1 kHz at -20 dBFS comes out at %.4f dB\n", 20 * std::log10(a / 0.1));
        CHECK(std::fabs(20 * std::log10(a / 0.1)) < 0.05, "defaults change a 1 kHz sine by %.3f dB", 20 * std::log10(a / 0.1));
    }
    // 4. the parameters reach the filter: both resonances up, cutoff 500 Hz, SPACING +1 -> VCF 2 rings at 500 Hz (R), VCF 1 at 1 kHz (L)
    {
        P p; p.prepareToPlay(48000, 512);
        set(p, P::Cutoff, 500); set(p, P::Res1, 1); set(p, P::Res2, 1); set(p, P::Spacing, 1);
        auto o = run(p, 300, [](int64_t i, int) { return i < 48 ? 0.1f : 0.0f; });
        const double fl = freq(o[0], 96000), fr = freq(o[1], 96000);
        std::printf("plugin: cutoff 500 Hz, spacing +1, both resonances 1: L %.1f Hz, R %.1f Hz\n", fl, fr);
        CHECK(std::fabs(fr / 500.0 - 1) < 0.03 && std::fabs(fl / 1000.0 - 1) < 0.03, "self-oscillation L %.1f R %.1f", fl, fr);
        CHECK(std::fabs(p.vcf1Hz.load() - 1000.0f) < 1.0f && std::fabs(p.vcf2Hz.load() - 500.0f) < 1.0f, "reported cutoffs %.1f / %.1f", p.vcf1Hz.load(), p.vcf2Hz.load());
        FiltaerEditor ed(p); ed.refreshStatus();
        CHECK(ed.statusText().contains("VCF 1 LP 1.00 kHz") && ed.statusText().contains("self-oscillating: VCF 1 + VCF 2"), "status '%s'", ed.statusText().toRawUTF8());
        set(p, P::Mode, 0); ed.refreshStatus();
        CHECK(ed.statusText().contains("VCF 1 HP"), "series status '%s'", ed.statusText().toRawUTF8());
    }
    // 5. mono input (Live's mono track): the one input channel feeds both filters, both outputs carry signal
    {
        P p; p.setPlayConfigDetails(1, 2, 48000, 512); p.prepareToPlay(48000, 512); set(p, P::Cutoff, 2000);
        auto o = run(p, 20, [](int64_t i, int) { return noise(i); }, 1);
        double l = 0, r = 0; for (size_t i = 0; i < o[0].size(); ++i) { l += o[0][i] * o[0][i]; r += o[1][i] * o[1][i]; }
        CHECK(p.getTotalNumInputChannels() == 1 && l > 1.0 && std::fabs(l / r - 1.0) < 1e-6, "mono in: L %.3f R %.3f (in channels %d)", l, r, p.getTotalNumInputChannels());
    }
    // 6. state round trip
    {
        P a, b; set(a, P::Cutoff, 333); set(a, P::Res1, 0.7f); set(a, P::Spacing, -2.5f); set(a, P::Mode, 2); set(a, P::Drive, 6); set(a, P::Mix, 0.4f);
        juce::MemoryBlock mb; a.getStateInformation(mb); b.setStateInformation(mb.getData(), (int) mb.getSize());
        for (int i = 0; i < P::NumParams; ++i) CHECK(std::abs(a.param(i).getValue() - b.param(i).getValue()) < 1e-6f, "state: %s differs", P::paramId(i));
    }
    // 7. editor: every control inside the window, no overlaps
    {
        P p; FiltaerEditor ed(p);
        juce::Array<juce::Rectangle<int>> rs;
        for (auto* c : ed.getChildren()) {
            CHECK(ed.getLocalBounds().contains(c->getBounds()), "editor: %s outside the window", c->getBounds().toString().toRawUTF8());
            for (auto& o : rs) CHECK(!o.intersects(c->getBounds()), "editor: overlap at %s", c->getBounds().toString().toRawUTF8());
            rs.add(c->getBounds());
        }
        if (argc > 1) {
            const auto img = ed.createComponentSnapshot(ed.getLocalBounds(), true, 2.0f);
            juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])); out.deleteFile();
            juce::FileOutputStream os(out);
            CHECK(os.openedOk() && juce::PNGImageFormat().writeImageToStream(img, os), "snapshot: write failed");
            std::printf("snapshot %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), ed.getWidth(), ed.getHeight());
        }
    }
    std::printf(fails ? "filtaer plugin tests: %d FAILED\n" : "filtaer plugin tests: all passed\n", fails);
    return fails ? 1 : 0;
}
