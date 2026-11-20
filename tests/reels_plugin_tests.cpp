// Reels plugin checks without a host: parameter order, empty reel = input bit-exact, record and play back through the processor
// with a host playhead, editor triggers, Rec released when refused, state round trip, editor geometry. Optional:
// reels_plugin_tests out.png writes an offscreen snapshot of the editor (agents look at the UI this way).
#include <JuceHeader.h>
#include "ReelsProcessor.h"
#include "ReelsEditor.h"
#include "BuildInfo.h"
#include <cstdio>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Head : juce::AudioPlayHead {  // Live-like transport: ppq from a sample counter, playing flag, tempo
    double sr = 48000, bpm = 120; int64_t pos = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo i; i.setBpm(bpm); i.setIsPlaying(playing); i.setPpqPosition((double) pos * bpm / (60.0 * sr));
        i.setTimeInSamples(pos); return i;
    }
};
static void set(ReelsProcessor& p, int prm, float plain) { auto& q = p.param(prm); q.setValueNotifyingHost(q.convertTo0to1(plain)); }

struct Rig {
    ReelsProcessor p; Head head; int N = 512; juce::AudioBuffer<float> b { 2, 512 }; juce::MidiBuffer m;
    std::vector<float> outL;
    Rig() { p.setPlayHead(&head); p.prepareToPlay(48000, 512); }
    // n blocks; input sample = f(host position)
    template <class F> void run(int blocks, F f) {
        for (int j = 0; j < blocks; ++j) {
            for (int i = 0; i < N; ++i) { const float x = f(head.pos + i); b.setSample(0, i, x); b.setSample(1, i, -x); }
            p.processBlock(b, m); m.clear();
            for (int i = 0; i < N; ++i) outL.push_back(b.getSample(0, i));
            if (head.playing) head.pos += N;
        }
    }
};
static float noise(int64_t n) { uint32_t s = (uint32_t) (n * 2654435761u + 7u); s ^= s >> 15; s *= 2246822519u; s ^= s >> 13; return (float) ((s & 0xFFFF) / 65536.0 - 0.5) * 0.4f; }

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    // 1. host order and ids
    {
        ReelsProcessor p;
        const char* want[] = {"rec", "undo", "redo", "clear", "bars", "reel_level", "input_level", "nudge"};
        CHECK(p.getParameters().size() == ReelsProcessor::NumParams, "%d parameters, want %d", p.getParameters().size(), (int) ReelsProcessor::NumParams);
        for (int i = 0; i < ReelsProcessor::NumParams; ++i) {
            auto* q = dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[i]);
            CHECK(q && q->getParameterID() == want[i], "slot %d is %s, want %s", i, q ? q->getParameterID().toRawUTF8() : "?", want[i]);
        }
        CHECK(p.getLatencySamples() == 0, "latency %d", p.getLatencySamples());
    }
    // 2. empty reel: output == input, bit-exact (both channels)
    {
        Rig r; int bad = 0;
        for (int j = 0; j < 40; ++j) {
            for (int i = 0; i < r.N; ++i) { r.b.setSample(0, i, noise(j * r.N + i)); r.b.setSample(1, i, noise(j * r.N + i + 99)); }
            r.p.processBlock(r.b, r.m);
            for (int i = 0; i < r.N; ++i) bad += r.b.getSample(0, i) != noise(j * r.N + i) || r.b.getSample(1, i) != noise(j * r.N + i + 99);
            r.head.pos += r.N;
        }
        CHECK(bad == 0, "empty reel: %d samples differ from the input", bad);
    }
    // 3. record 1 bar at 120 BPM / 48 kHz (96000 samples = 187.5 blocks), input off, pass 2 replays it (input level 0 -> reel only)
    {
        Rig r; set(r.p, ReelsProcessor::Bars, 0); set(r.p, ReelsProcessor::Rec, 1);
        r.run(188, [](int64_t n) { return n < 96000 ? noise(n) : 0.0f; });
        set(r.p, ReelsProcessor::Rec, 0);
        r.run(400, [](int64_t) { return 0.0f; });
        int bad = 0; for (int64_t m = 300; m < 96000 - 300; ++m) bad += r.outL[(size_t) (2 * 96000 + m)] != noise(m);
        CHECK(bad == 0, "record/play: %d samples of pass 2 differ from the recording (want bit-exact)", bad);
        CHECK(r.p.status.layers.load() == 1 && !r.p.status.recording.load(), "record/play: layers %d recording %d", r.p.status.layers.load(), (int) r.p.status.recording.load());
        // editor Undo -> silence, Redo -> back
        r.p.undoRequest.store(true); r.run(20, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.heard.load() == 0, "undo request: heard %d", r.p.status.heard.load());
        double peak = 0; for (size_t i = r.outL.size() - 5000; i < r.outL.size(); ++i) peak = std::max(peak, (double) std::fabs(r.outL[i]));
        CHECK(peak == 0.0, "undo request: reel still audible (%g)", peak);
        r.p.redoRequest.store(true); r.run(20, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.heard.load() == 1, "redo request: heard %d", r.p.status.heard.load());
        // host-mapped Undo parameter: rising edge fires once
        set(r.p, ReelsProcessor::Undo, 1); r.run(4, [](int64_t) { return 0.0f; }); set(r.p, ReelsProcessor::Undo, 0); r.run(2, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.heard.load() == 0, "undo parameter: heard %d", r.p.status.heard.load());
        // reset() (host) keeps the layers
        r.p.reset(); r.run(2, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.layers.load() == 1, "reset: layers %d, want kept", r.p.status.layers.load());
        // Clear empties the reel
        r.p.clearRequest.store(true); r.run(10, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.layers.load() == 0, "clear: layers %d", r.p.status.layers.load());
    }
    // 4. transport stopped: Rec arms, recording starts with play; the editor says so
    {
        Rig r; r.head.playing = false; set(r.p, ReelsProcessor::Rec, 1);
        r.run(4, [](int64_t) { return 0.1f; });
        CHECK(!r.p.status.recording.load(), "stopped: recording without transport");
        {
            ReelsEditor ed(r.p); ed.refreshStatus();
            CHECK(ed.warningText().contains("Rec armed"), "stopped: warning '%s'", ed.warningText().toRawUTF8());
        }
        r.head.playing = true; r.run(4, [](int64_t) { return 0.1f; });
        CHECK(r.p.status.recording.load(), "play: not recording");
    }
    // 5. Undo during a take drops it and the editor releases Rec
    {
        Rig r; set(r.p, ReelsProcessor::Rec, 1); r.run(20, [](int64_t) { return 0.1f; });
        r.p.undoRequest.store(true); r.run(2, [](int64_t) { return 0.1f; });
        ReelsEditor ed(r.p); ed.refreshStatus();
        CHECK(ed.statusText().contains(juce::String("build ") + TIDBANK_BUILD), "status names the build: %s", ed.statusText().toRawUTF8());
        CHECK(r.p.param(ReelsProcessor::Rec).getValue() < 0.5f, "undo during take: Rec still on");
        r.run(4, [](int64_t) { return 0.1f; });
        CHECK(!r.p.status.recording.load() && r.p.status.heard.load() == 0, "undo during take: recording %d heard %d", (int) r.p.status.recording.load(), r.p.status.heard.load());
    }
    // 6. no host transport (Standalone): free running, said in the editor
    {
        ReelsProcessor p; p.prepareToPlay(48000, 512); juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; b.clear(); p.processBlock(b, m);
        ReelsEditor ed(p); ed.refreshStatus();
        CHECK(ed.warningText().contains("No host transport"), "no host: warning '%s'", ed.warningText().toRawUTF8());
    }
    // 7. state round trip (parameters only; the reel audio is not saved, plan Q8)
    {
        ReelsProcessor a, b; set(a, ReelsProcessor::Bars, 2); set(a, ReelsProcessor::ReelLevel, 0.3f); set(a, ReelsProcessor::Rec, 1);
        juce::MemoryBlock mb; a.getStateInformation(mb); b.setStateInformation(mb.getData(), (int) mb.getSize());
        for (int i = 0; i < ReelsProcessor::NumParams; ++i)
            CHECK(std::abs(a.param(i).getValue() - b.param(i).getValue()) < 1e-6f, "state: %s differs", ReelsProcessor::paramId(i));
    }
    // 9. host-mapped Nudge: -1 brakes the reel to a stop; back at 0 the motor returns it to 1x; the editor shows the offset
    {
        Rig r; set(r.p, ReelsProcessor::Bars, 0); set(r.p, ReelsProcessor::Rec, 1);
        r.run(188, [](int64_t n) { return n < 96000 ? noise(n) : 0.0f; });
        set(r.p, ReelsProcessor::Rec, 0); set(r.p, ReelsProcessor::Nudge, -1.0f);
        r.run(47, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.speed.load() < 1e-3f, "nudge -1: speed %g after 0.5 s", r.p.status.speed.load());
        set(r.p, ReelsProcessor::Nudge, 0.0f); r.run(188, [](int64_t) { return 0.0f; });
        CHECK(r.p.status.speed.load() == 1.0f && r.p.status.offsetMs.load() < -300.0f, "nudge released: speed %g offset %g ms",
              r.p.status.speed.load(), r.p.status.offsetMs.load());
        ReelsEditor ed(r.p); ed.refreshStatus();
        CHECK(ed.statusText().contains("off the grid"), "nudge: status '%s'", ed.statusText().toRawUTF8());
    }
    // 8. editor: every control inside the window, no overlaps
    {
        ReelsProcessor p; ReelsEditor ed(p);
        juce::Array<juce::Rectangle<int>> rs;
        for (auto* c : ed.getChildren()) {
            CHECK(ed.getLocalBounds().contains(c->getBounds()), "editor: %s outside the window", c->getName().toRawUTF8());
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
    std::printf(fails ? "reels plugin tests: %d FAILED\n" : "reels plugin tests: all passed\n", fails);
    return fails ? 1 : 0;
}
