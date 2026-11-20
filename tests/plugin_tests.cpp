// Plugin-level checks without a host: parameter order (Move/Push banks), state round trip, bit-exact dry path
// with echo at 0 (zero latency), finite runaway, panic to exact silence, BBD echo timing and engine/profile switches.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <vector>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void block(TidBankProcessor& p, juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { p.processBlock(b, m); m.clear(); }

#ifdef TIDBANK_DEBUG_PROBE
// Debug probe through the real processor: enabled by env var (1 s) or by a config file (5 s), log drained by the Timer
// and on destruction. Every delay line's diff must be within 1 sample. Logs stay under $TMPDIR/tidbank-probe (never the user's real log).
static juce::StringArray probeRun(const char* how, double interval, double seconds, juce::File dir) {
    const juce::File logf = dir.getChildFile(juce::String("probe-") + juce::String((int) interval) + "s.log"), cfg = dir.getChildFile("probe.cfg");
    logf.deleteFile(); cfg.deleteFile();
    setenv("TIDBANK_PROBE_LOG", logf.getFullPathName().toRawUTF8(), 1);
    unsetenv("TIDBANK_PROBE"); unsetenv("TIDBANK_PROBE_CFG");
    if (juce::String(how) == "env") setenv("TIDBANK_PROBE", juce::String(interval).toRawUTF8(), 1);
    else { cfg.replaceWithText(juce::String("interval=") + juce::String(interval) + "\n"); setenv("TIDBANK_PROBE_CFG", cfg.getFullPathName().toRawUTF8(), 1); }
    const int N = 512; juce::Random rng(7);
    juce::StringArray mid;
    {
        TidBankProcessor p; p.prepareToPlay(48000.0, N);
        juce::AudioBuffer<float> buf(2, N); juce::MidiBuffer midi;
        const int blocks = (int) (seconds * 48000.0 / N);
        for (int j = 0; j < blocks; ++j) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) buf.setSample(c, i, j % 47 == 0 && i == 0 ? 1.0f : 0.02f * (rng.nextFloat() - 0.5f));
            block(p, buf, midi);
        }
        // the Timer drains on the message thread: give it one period (a real host runs this loop while audio plays)
        juce::MessageManager::getInstance()->runDispatchLoopUntil((int) (interval * 1000.0) + 400);
        logf.readLines(mid);  // what the Timer wrote before the processor goes away
        // the editor's probe strip reads these: tape head 2 (head 1 is unused in the default mode), BBD out and CPU must be
        // filled, each with measured == computed within a sample
        const auto& lv = p.probeLive();
        CHECK(lv.on.load() && lv.seenMs[1].load() != 0 && lv.seenMs[3].load() != 0 && lv.cpuSeenMs.load() != 0,
              "probe %s: live view not filled (on=%d h2=%u bbd=%u cpu=%u)", how, (int) lv.on.load(), lv.seenMs[1].load(), lv.seenMs[3].load(), lv.cpuSeenMs.load());
        for (int k : { 1, 3 })
            CHECK(std::abs(lv.measured[k].load() - lv.computed[k].load()) < 1.0 && lv.measured[k].load() > 0.0,
                  "probe %s: live slot %d measured %.3f vs computed %.3f", how, k, lv.measured[k].load(), lv.computed[k].load());
    }
    juce::StringArray all; logf.readLines(all);
    CHECK(mid.size() > 1 && mid.size() <= all.size(), "probe %s %gs: the Timer wrote nothing before destruction (%d lines)", how, interval, mid.size());
    int tape2 = 0, tape3 = 0, bbdOut = 0, cpu = 0, blk = 0; double worst = 0.0;
    for (auto& l : all) {
        if (l.startsWith("delay")) {
            worst = std::max(worst, std::abs(l.fromFirstOccurrenceOf("diff=", false, false).getDoubleValue()));
            if (l.startsWith("delay tape")) { if (l.contains(" head 2 ")) ++tape2; else if (l.contains(" head 3 ")) ++tape3; }
            else if (l.startsWith("delay m8192")) ++bbdOut;
        } else if (l.startsWith("cpu")) ++cpu; else if (l.startsWith("block")) ++blk;
    }
    std::printf("probe %s %gs: %d lines, tape h2=%d h3=%d, bbd out=%d, cpu=%d, block=%d, max|diff|=%.5f smp -> %s\n", how, interval, all.size(), tape2, tape3,
                bbdOut, cpu, blk, worst, logf.getFullPathName().toRawUTF8());
    CHECK(all.size() > 0 && all[0].startsWith("# TidBank probe start"), "probe %s %gs: log header missing", how, interval);
    CHECK(tape2 >= 1 && tape3 >= 1, "probe %s %gs: need a tape record per active head (mode 4 = heads 2+3): h2=%d h3=%d", how, interval, tape2, tape3);
    CHECK(bbdOut >= 1, "probe %s %gs: no BBD record", how, interval);
    CHECK(cpu >= 1 && blk >= 1, "probe %s %gs: need cpu and block lines (cpu=%d block=%d)", how, interval, cpu, blk);
    CHECK(worst <= 1.0, "probe %s %gs: measured vs computed differ by %.4f samples", how, interval, worst);
    unsetenv("TIDBANK_PROBE"); unsetenv("TIDBANK_PROBE_CFG"); unsetenv("TIDBANK_PROBE_LOG");
    return all;
}
#endif

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const int N = 512;
    TidBankProcessor p;
    p.prepareToPlay(48000.0, N);
    juce::AudioBuffer<float> buf(2, N); juce::MidiBuffer midi;

    // 1. Host parameter order = enum order, bank 1 (first 8 knobs on Move/Push) = performance controls (d20).
    const auto& hp = p.getParameters();
    CHECK(hp.size() == TidBankProcessor::NumParams, "host sees %d params, want %d (no hidden extras)", hp.size(), (int) TidBankProcessor::NumParams);
    for (int i = 0; i < TidBankProcessor::NumParams && i < hp.size(); ++i)
        CHECK(hp[i] == &p.param(i), "host param %d is not %s", i, TidBankProcessor::paramId(i));
    const char* bank1[] = {"intensity", "division", "mode", "nudge_cents", "inertia_ms", "fb_lpf", "fb_hpf", "echo_level"};
    for (int i = 0; i < 8; ++i)
        CHECK(juce::String(TidBankProcessor::paramId(i)) == bank1[i], "bank 1 knob %d is %s, want %s", i + 1, TidBankProcessor::paramId(i), bank1[i]);
    CHECK(!p.acceptsMidi(), "plugin must not take MIDI (d20: the host maps controllers)");
    const char* rest[] = {"time_ms", "drive", "spacing", "head_target", "sync", "dry_level", "output", "panic",  // bank 2 (no Tempo)
                          "engine", "bbd_profile", "bbd_bright", "bbd_spacing", "ping_pong", "bbd_droop"};       // bank 3
    CHECK(TidBankProcessor::NumParams == 8 + 14, "NumParams is %d, want 22", (int) TidBankProcessor::NumParams);
    for (int i = 0; i < 14; ++i)
        CHECK(juce::String(TidBankProcessor::paramId(8 + i)) == rest[i], "bank %d knob %d is %s, want %s", 2 + i / 8, i % 8 + 1, TidBankProcessor::paramId(8 + i), rest[i]);
    CHECK(!p.param(TidBankProcessor::BbdProfile).isAutomatable(), "BBD profile must not be automatable (d21)");

    // 2. Save/restore keeps every parameter value.
    for (int i = 0; i < TidBankProcessor::NumParams; ++i) p.param(i).setValueNotifyingHost(i == TidBankProcessor::BbdDroop ? 1.0f : i == TidBankProcessor::Panic || i == TidBankProcessor::Sync || i == TidBankProcessor::BbdBright || i == TidBankProcessor::PingPong ? 0.0f : 0.37f);  // bools and the 2-way droop choice: a real toggle (sync on -> off, droop Authentic -> Compensated)
    juce::MemoryBlock state; p.getStateInformation(state);
    TidBankProcessor q; q.setStateInformation(state.getData(), (int) state.getSize());
    for (int i = 0; i < TidBankProcessor::NumParams; ++i)
        CHECK(std::abs(q.param(i).getValue() - p.param(i).getValue()) < 1e-6f, "restored %s: %f vs %f", TidBankProcessor::paramId(i),
              (double) q.param(i).getValue(), (double) p.param(i).getValue());

    // 5. Fresh processor, echo 0: output equals input exactly (dry path untouched, zero latency); with echo, finite.
    TidBankProcessor r; r.prepareToPlay(48000.0, N);
    r.param(TidBankProcessor::Echo).setValueNotifyingHost(0.0f);
    juce::Random rng(7); float maxDiff = 0.0f;
    for (int k = 0; k < 20; ++k) {
        juce::AudioBuffer<float> in(2, N);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) in.setSample(c, i, rng.nextFloat() - 0.5f);
        buf.makeCopyOf(in); block(r, buf, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) maxDiff = std::max(maxDiff, std::abs(buf.getSample(c, i) - in.getSample(c, i)));
    }
    CHECK(maxDiff == 0.0f, "dry path differs by %g with echo 0", (double) maxDiff);
    r.param(TidBankProcessor::Echo).setValueNotifyingHost(1.0f);
    r.param(TidBankProcessor::Intensity).setValueNotifyingHost(1.0f);  // normalised 1 = intensity 1.4 (runaway, d36)
    bool finite = true;
    for (int k = 0; k < 2000; ++k) {  // ~21 s
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) buf.setSample(c, i, k < 100 ? rng.nextFloat() - 0.5f : 0.0f);
        block(r, buf, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) finite = finite && std::isfinite(buf.getSample(c, i));
    }
    CHECK(finite, "non-finite output at intensity 1.4");

    // 6. Panic: runaway at intensity 1.4, dry 0. After the 10 ms fade the echo is erased: exact silence.
    r.param(TidBankProcessor::Dry).setValueNotifyingHost(0.0f);
    float before = 0.0f;
    for (int k = 0; k < 400; ++k) {  // ~4 s of self-oscillation, no input after the first block
        for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) buf.setSample(c, i, k == 0 ? rng.nextFloat() - 0.5f : 0.0f);
        block(r, buf, midi);
        if (k > 300) before = std::max(before, buf.getMagnitude(0, N));
    }
    r.panicRequest.store(true);
    float tail = 0.0f;
    for (int k = 0; k < 20; ++k) {
        buf.clear(); block(r, buf, midi);
        if (k >= 1) tail = std::max(tail, buf.getMagnitude(0, N));  // block 0 holds the 10 ms fade (480 < 512 samples)
    }
    CHECK(before > 0.01f && tail == 0.0f, "panic: level before %g, after fade %g (want > 0.01, exactly 0)", (double) before, (double) tail);
    // Same through the learnable parameter (rising edge).
    for (int k = 0; k < 200; ++k) { for (int i = 0; i < N; ++i) { buf.setSample(0, i, k == 0 ? 0.5f : 0.0f); buf.setSample(1, i, 0.0f); } block(r, buf, midi); }
    r.param(TidBankProcessor::Panic).setValueNotifyingHost(1.0f);
    tail = 0.0f;
    for (int k = 0; k < 10; ++k) { buf.clear(); block(r, buf, midi); if (k >= 1) tail = std::max(tail, buf.getMagnitude(0, N)); }
    CHECK(tail == 0.0f, "panic parameter: after fade %g", (double) tail);

    // 7. BBD engine through the plugin: echo lands at Time (+ the chain's impulse-response peak) for every profile;
    //    runaway at intensity 1.2 (range max 1.4 since d36) stays finite; switching engine and profile while running stays finite.
    for (int k = 0; k < 3; ++k) {
        TidBankProcessor b; b.prepareToPlay(48000.0, N);
        auto set = [&](int prm, float v) { auto& x = b.param(prm); x.setValueNotifyingHost(x.convertTo0to1(v)); };
        set(TidBankProcessor::Engine, 1); set(TidBankProcessor::BbdProfile, (float) k); set(TidBankProcessor::Sync, 0);
        set(TidBankProcessor::TimeMs, 300.0f); set(TidBankProcessor::Intensity, 0.0f); set(TidBankProcessor::Dry, 0.0f); set(TidBankProcessor::Echo, 1.0f);
        for (int j = 0; j < 20; ++j) { buf.clear(); block(b, buf, midi); }  // engine and profile fades settle (~0.2 s)
        float pk = 0.0f; int at = -1, n0 = 0;
        for (int j = 0; j < 40; ++j) {
            buf.clear(); if (j == 0) { buf.setSample(0, 0, 1.0f); buf.setSample(1, 0, 1.0f); }
            block(b, buf, midi);
            for (int i = 0; i < N; ++i) if (std::abs(buf.getSample(0, i)) > pk) { pk = std::abs(buf.getSample(0, i)); at = n0 + i; }
            n0 += N;
        }
        const double ms = at * 1000.0 / 48000.0, want = 300.0 + (k == 0 ? 0.458 : 0.292);  // ir_peak_ms from --dump-profile
        // 1.5 ms: the compander is on here, so a lone impulse meets the compressor's idle gain (+40 dB) and clips in the
        // BBD, which moves the echo peak by up to ~0.8 ms. Exact timing (compander off) is ears b1.
        CHECK(std::abs(ms - want) < 1.5, "BBD profile %d: echo at %.3f ms, want %.3f", k, ms, want);
        set(TidBankProcessor::Intensity, 1.2f);
        bool fin = true;
        for (int j = 0; j < 500; ++j) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) buf.setSample(c, i, j < 20 ? rng.nextFloat() - 0.5f : 0.0f);
            if (j == 200) set(TidBankProcessor::Engine, 0); if (j == 260) set(TidBankProcessor::Engine, 1);
            if (j == 320) set(TidBankProcessor::BbdProfile, (float) ((k + 1) % 3));
            block(b, buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < N; ++i) fin = fin && std::isfinite(buf.getSample(c, i));
        }
        CHECK(fin, "BBD profile %d: non-finite output (runaway / engine or profile switch)", k);
    }

    {   // 7b. BBD hold droop selector reaches the engine (d36): D-4096, Time 300 ms, Intensity 1.1, one pulse, 15 s.
        //     Authentic: the hold leaves the loop at ~0.84 -> decays. Compensated: loop ~1.1 -> runs away.
        auto lastSecondPeak = [&](float droop) {
            TidBankProcessor b; b.prepareToPlay(48000.0, N);
            auto set = [&](int prm, float v) { auto& x = b.param(prm); x.setValueNotifyingHost(x.convertTo0to1(v)); };
            set(TidBankProcessor::Engine, 1); set(TidBankProcessor::BbdProfile, 1); set(TidBankProcessor::Sync, 0);
            set(TidBankProcessor::TimeMs, 300.0f); set(TidBankProcessor::Intensity, 1.1f); set(TidBankProcessor::Dry, 0.0f);
            set(TidBankProcessor::Echo, 1.0f); set(TidBankProcessor::BbdDroop, droop);
            const int blocks = (int) (15.0 * 48000.0 / N), last = (int) (1.0 * 48000.0 / N);
            float pk = 0.0f;
            for (int j = 0; j < blocks; ++j) {
                buf.clear(); if (j == 20) { buf.setSample(0, 0, 0.5f); buf.setSample(1, 0, 0.5f); }
                block(b, buf, midi);
                if (j >= blocks - last) for (int i = 0; i < N; ++i) pk = std::max(pk, std::abs(buf.getSample(0, i)));
            }
            return 20.0 * std::log10(pk + 1e-30);
        };
        const double auth = lastSecondPeak(0.0f), comp = lastSecondPeak(1.0f);
        std::printf("BBD hold droop D-4096 300 ms I=1.1, last second: authentic %.1f dBFS, compensated %.1f dBFS\n", auth, comp);
        CHECK(auth < -60.0, "authentic droop should decay at 300 ms / 1.1: %.1f dBFS", auth);
        CHECK(comp > -20.0, "compensated droop should run away at 300 ms / 1.1: %.1f dBFS", comp);
    }

#ifdef TIDBANK_DEBUG_PROBE
    {   // 8. debug probe (compiled in only with TIDBANK_DEBUG_PROBE)
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("tidbank-probe"); dir.createDirectory();
        probeRun("env", 1.0, 6.0, dir);
        probeRun("cfg", 5.0, 12.0, dir);
    }
#endif

    {   // 10. BBD spacing and ping-pong (Matriarch-style, ref #13), M-8192, dry 0, echo 1. The right line keeps Time; the left
        //     line is Time x 2^(spacing/100) free, or steps through the divisions synced. Ping-pong: mono sum into the left line,
        //     L -> R -> L. Echo positions in ms, +0.458 ms = this profile's impulse-response peak (as in section 7).
        auto run = [&](float spacing, bool pingPong, bool sync, float intensity, bool leftOnly, std::vector<float>& outL, std::vector<float>& outR) {
            TidBankProcessor b; b.prepareToPlay(48000.0, N);
            auto set = [&](int prm, float v) { auto& x = b.param(prm); x.setValueNotifyingHost(x.convertTo0to1(v)); };
            set(TidBankProcessor::Engine, 1); set(TidBankProcessor::BbdProfile, 0); set(TidBankProcessor::Sync, sync ? 1.0f : 0.0f);
            set(TidBankProcessor::TimeMs, 300.0f); set(TidBankProcessor::Division, 5);  // 1/8 = 250 ms
            set(TidBankProcessor::Intensity, intensity); set(TidBankProcessor::Dry, 0.0f); set(TidBankProcessor::Echo, 1.0f);
            set(TidBankProcessor::BbdSpacing, spacing); set(TidBankProcessor::PingPong, pingPong ? 1.0f : 0.0f);
            for (int j = 0; j < 20; ++j) { buf.clear(); block(b, buf, midi); }
            outL.clear(); outR.clear();
            for (int j = 0; j < 130; ++j) {  // ~1.4 s
                buf.clear(); if (j == 0) { buf.setSample(0, 0, 0.5f); if (!leftOnly) buf.setSample(1, 0, 0.5f); }
                block(b, buf, midi);
                for (int i = 0; i < N; ++i) { outL.push_back(buf.getSample(0, i)); outR.push_back(buf.getSample(1, i)); }
            }
        };
        auto peakMs = [](const std::vector<float>& x, double fromMs, double toMs, float* level = nullptr) {
            const int a = (int) (fromMs * 48.0), z = std::min((int) x.size(), (int) (toMs * 48.0));
            int at = a; float pk = 0.0f;
            for (int i = a; i < z; ++i) if (std::abs(x[(size_t) i]) > pk) { pk = std::abs(x[(size_t) i]); at = i; }
            if (level) *level = pk;
            return at / 48.0;
        };
        std::vector<float> l, r; const double ir = 0.458;
        run(100.0f, false, false, 0.0f, false, l, r);   // free, +100 %: left at 2 x 300 ms, right at 300 ms
        CHECK(std::abs(peakMs(l, 0, 1300) - (600.0 + ir)) < 1.5 && std::abs(peakMs(r, 0, 1300) - (300.0 + ir)) < 1.5,
              "spacing +100%%: left %.2f ms (want 600), right %.2f ms (want 300)", peakMs(l, 0, 1300), peakMs(r, 0, 1300));
        run(-100.0f, false, false, 0.0f, false, l, r);  // free, -100 %: left at half
        CHECK(std::abs(peakMs(l, 0, 1300) - (150.0 + ir)) < 1.5, "spacing -100%%: left %.2f ms (want 150)", peakMs(l, 0, 1300));
        run(50.0f, false, true, 0.0f, false, l, r);     // synced 1/8 at 120 BPM, +50 % = +3 divisions -> 1/4 = 500 ms; right 250 ms
        CHECK(std::abs(peakMs(l, 0, 1300) - (500.0 + ir)) < 1.5 && std::abs(peakMs(r, 0, 1300) - (250.0 + ir)) < 1.5,
              "synced spacing +50%%: left %.2f ms (want 500), right %.2f ms (want 250)", peakMs(l, 0, 1300), peakMs(r, 0, 1300));
        float rLvl = 0.0f;
        run(0.0f, false, false, 0.5f, true, l, r);      // no ping-pong, input left only: the right side stays silent
        peakMs(r, 0, 1300, &rLvl);
        CHECK(rLvl < 1e-5f, "ping-pong off, left-only input: right side %g (want only BBD chip noise, < -100 dB)", (double) rLvl);
        run(0.0f, true, false, 0.5f, true, l, r);       // ping-pong: L at 300, R at 600, L again at 900
        float l1, r0, r1, l2;
        const double tl1 = peakMs(l, 0, 450, &l1), tr1 = peakMs(r, 450, 750, &r1), tl2 = peakMs(l, 750, 1050, &l2);
        peakMs(r, 0, 450, &r0);
        CHECK(std::abs(tl1 - (300 + ir)) < 1.5 && std::abs(tr1 - (600 + 2 * ir)) < 2.0 && std::abs(tl2 - (900 + 3 * ir)) < 2.5,
              "ping-pong: L %.2f, R %.2f, L %.2f ms (want 300, 600, 900)", tl1, tr1, tl2);
        CHECK(r0 < 0.01f * l1 && r1 > 0.1f * l1 && l2 < r1, "ping-pong levels: L1 %g, R before %g, R1 %g, L2 %g", (double) l1, (double) r0, (double) r1, (double) l2);
        std::printf("ping-pong: L %.2f ms (%.3f), R %.2f ms (%.3f), L %.2f ms (%.3f)\n", tl1, (double) l1, tr1, (double) r1, tl2, (double) l2);
    }

    {   // 10b. Left-only input, both engines, ping-pong off, Drive up: the right output must stay (near) silent. the listener heard a
        //      "ghost" of a clap on the silent side; this bounds what the plugin itself puts there.
        for (int eng = 0; eng < 2; ++eng) {
            TidBankProcessor b; b.prepareToPlay(48000.0, N);
            auto set = [&](int prm, float v) { auto& x = b.param(prm); x.setValueNotifyingHost(x.convertTo0to1(v)); };
            set(TidBankProcessor::Engine, (float) eng); set(TidBankProcessor::Intensity, 0.9f); set(TidBankProcessor::Drive, 12.0f);
            set(TidBankProcessor::Echo, 1.0f); set(TidBankProcessor::Dry, 1.0f);
            float peakL = 0.0f, peakR = 0.0f;
            for (int j = 0; j < 200; ++j) {  // ~2.1 s of clap-like bursts on the left only
                buf.clear();
                if (j % 40 < 2) for (int i = 0; i < N; ++i) buf.setSample(0, i, (float) ((i * 7919 % 1000) / 500.0 - 1.0) * 0.9f);
                block(b, buf, midi);
                for (int i = 0; i < N; ++i) { peakL = std::max(peakL, std::abs(buf.getSample(0, i))); peakR = std::max(peakR, std::abs(buf.getSample(1, i))); }
            }
            CHECK(peakR < 1e-5f, "%s, left-only input: right peak %g (left %g), want < -100 dBFS", eng ? "BBD" : "Tape", (double) peakR, (double) peakL);
            std::printf("crosstalk %s: left peak %.3f, right peak %.3g (%.1f dBFS)\n", eng ? "BBD" : "Tape", (double) peakL, (double) peakR,
                        peakR > 0.0f ? 20.0 * std::log10((double) peakR) : -999.0);
        }
    }

    {   // 10c. Nudge acts with Sync off on the BBD too (as on tape): Time 300 ms, Nudge -400 ct -> 300 x 2^(1/3) = 378 ms.
        TidBankProcessor b; b.prepareToPlay(48000.0, N);
        auto set = [&](int prm, float v) { auto& x = b.param(prm); x.setValueNotifyingHost(x.convertTo0to1(v)); };
        set(TidBankProcessor::Engine, 1); set(TidBankProcessor::BbdProfile, 0); set(TidBankProcessor::Sync, 0.0f);
        set(TidBankProcessor::TimeMs, 300.0f); set(TidBankProcessor::Nudge, -400.0f);
        set(TidBankProcessor::Intensity, 0.0f); set(TidBankProcessor::Dry, 0.0f); set(TidBankProcessor::Echo, 1.0f);
        for (int j = 0; j < 20; ++j) { buf.clear(); block(b, buf, midi); }
        int arg = -1; float best = 0.0f;
        for (int j = 0; j < 60; ++j) {  // ~640 ms
            buf.clear(); if (j == 0) buf.setSample(0, 0, 0.5f);
            block(b, buf, midi);
            for (int i = 0; i < N; ++i) if (std::abs(buf.getSample(0, i)) > best) { best = std::abs(buf.getSample(0, i)); arg = j * N + i; }
        }
        const double ms = arg * 1000.0 / 48000.0, want = 300.0 * std::exp2(400.0 / 1200.0);
        CHECK(std::abs(ms - want) < 2.0, "BBD free nudge: echo at %.2f ms, want about %.2f", ms, want);
        std::printf("BBD free nudge -400 ct: echo at %.2f ms (want %.2f + IR peak)\n", ms, want);
    }

    {   // 11. Editor sections: one fixed window for both faces; a control is shown iff its section is on the face;
        //     shown controls sit inside the window and do not overlap.
        TidBankProcessor b;
        std::unique_ptr<juce::AudioProcessorEditor> ed(b.createEditor());
        auto* te = dynamic_cast<TidBankEditor*>(ed.get());
        CHECK(te != nullptr, "editor is not a TidBankEditor");
        juce::Rectangle<int> size[4];
        for (int f = 0; te != nullptr && f < 4; ++f) {
            const int bbd = f / 2, sync = f % 2;
            const char* name[4] = {"Tape/free", "Tape/sync", "BBD/free", "BBD/sync"};
            auto& eng = b.param(TidBankProcessor::Engine); eng.setValueNotifyingHost(eng.convertTo0to1((float) bbd));
            b.param(TidBankProcessor::Sync).setValueNotifyingHost((float) sync);
            te->refreshStatus();
            size[f] = te->getLocalBounds();
            std::vector<juce::Rectangle<int>> shown;
            for (int i = 0; i < TidBankProcessor::NumParams; ++i) {
                const auto& c = te->controlFor(i);
                CHECK(c.isVisible() == TidBankEditor::shown(i, bbd == 1, sync == 1), "%s: %s visible=%d", name[f],
                      TidBankProcessor::paramId(i), (int) c.isVisible());
                if (!c.isVisible()) continue;
                CHECK(te->getLocalBounds().contains(c.getBounds()) && !c.getBounds().isEmpty(), "%s: %s outside the window",
                      name[f], TidBankProcessor::paramId(i));
                shown.push_back(c.getBounds());
            }
            CHECK(te->controlFor(TidBankProcessor::Division).isVisible() != te->controlFor(TidBankProcessor::TimeMs).isVisible(),
                  "%s: exactly one of Division and Time must show", name[f]);
            for (size_t x = 0; x < shown.size(); ++x)
                for (size_t y = x + 1; y < shown.size(); ++y)
                    CHECK(!shown[x].intersects(shown[y]), "%s: controls %d and %d overlap", name[f], (int) x, (int) y);
        }
        for (int f = 1; f < 4; ++f)
            CHECK(size[f] == size[0], "window size differs: %dx%d vs %dx%d", size[f].getWidth(), size[f].getHeight(),
                  size[0].getWidth(), size[0].getHeight());
    }

    std::printf(fails ? "plugin tests: %d FAILED\n" : "plugin tests: all passed\n", fails);
    return fails ? 1 : 0;
}
