#pragma once
// Plugin side of the debug probe. Compiled only with TIDBANK_DEBUG_PROBE.
// Off until configured: env TIDBANK_PROBE=<seconds> (1, 5, any positive number; non-numeric = 1) or the config file
// ~/Library/Application Support/TidBank/probe.cfg (a number, or "interval=<seconds>"; empty = 1). Log: ~/Library/Logs/TidBank/probe.log
// (TIDBANK_PROBE_LOG / TIDBANK_PROBE_CFG override the paths, used by tests). Config and file I/O happen on the message thread only: the audio
// thread just pushes POD records into the SPSC ring (dsp/DebugProbe.h) and this Timer drains it (every 250 ms, or the interval if shorter).
// Draining also fills `live`, the latest delay/CPU values that the editor's probe strip shows; the editor only reads atomics.
#ifdef TIDBANK_DEBUG_PROBE
#include <JuceHeader.h>
#include "DebugProbe.h"
#include <cstdlib>
#include <atomic>
#include <mutex>

namespace tidbank { namespace probe {

class PluginProbe : private juce::Timer {
public:
    // Latest values per slot, written by drain(), read by the editor. Slots 0..2 = tape heads 1..3, 3 = BBD output (any profile).
    // seenMs = juce::Time::getMillisecondCounter() when the slot was last updated, 0 = never.
    struct LiveView {
        std::atomic<bool> on {false};
        std::atomic<double> sampleRate {48000.0}, interval {1.0};
        std::atomic<double> measured[4] {}, computed[4] {};
        std::atomic<uint32_t> seenMs[4] {};
        std::atomic<double> cpuAvg {0.0}, cpuPeak {0.0};
        std::atomic<uint64_t> drops {0};
        std::atomic<uint32_t> cpuSeenMs {0};
    };

    Hub hub; Tracer tape, bbd[3];
    bool enabled = false;
    LiveView live;

    ~PluginProbe() override { stopTimer(); const std::lock_guard<std::mutex> g(consumer); drain(); closeLog(); }

    // Message thread (prepareToPlay). Returns whether the probe runs; the caller attaches/detaches the tracers accordingly.
    // Hosts may call prepareToPlay off the message thread, and stopTimer() does not wait for a running callback, so
    // every consumer-side step (drain, log open/close, ring re-prepare) holds `consumer`. The audio thread never takes it.
    bool configure(double sr, const char* const bbdNames[3]) {
        stopTimer();
        const std::lock_guard<std::mutex> g(consumer);
        drain();
        const double iv = readInterval();
        enabled = iv > 0.0;
        live.on = enabled;
        for (auto& t : live.seenMs) t = 0;
        live.cpuSeenMs = 0;
        if (!enabled) { closeLog(); return false; }
        Hub::Config c; c.intervalSec = iv; c.firstSec = iv; c.capacity = 32768;
        hub.prepare(sr, c);
        tape.init(&hub, 0, "tape");
        for (int k = 0; k < 3; ++k) bbd[k].init(&hub, 1, bbdNames[k]);
        sampleRate = sr; interval = iv; live.sampleRate = sr; live.interval = iv;
        openLog();
        startTimer(std::max(1, (int) std::lround(std::min(iv, 0.25) * 1000.0)));
        return true;
    }

private:
    static double parseInterval(const juce::String& text) {  // "5", "interval=5", "" -> 1, "0" -> off
        auto t = text.trim(); if (t.startsWithIgnoreCase("interval")) t = t.fromFirstOccurrenceOf("=", false, false).trim();
        if (t.isEmpty()) return 1.0;
        if (!t.containsAnyOf("0123456789")) return 1.0;
        const double v = t.getDoubleValue(); return v > 0.0 ? v : 0.0;
    }
    static double readInterval() {
        if (const char* e = std::getenv("TIDBANK_PROBE")) return parseInterval(e);
        const char* ov = std::getenv("TIDBANK_PROBE_CFG");  // tests: a config file elsewhere than the user's real one
        const auto cfg = ov ? juce::File(juce::String::fromUTF8(ov))
                            : juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("TidBank/probe.cfg");
        return cfg.existsAsFile() ? parseInterval(cfg.loadFileAsString()) : 0.0;
    }
    static juce::File logPath() {
        if (const char* p = std::getenv("TIDBANK_PROBE_LOG")) return juce::File(juce::String::fromUTF8(p));
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Logs/TidBank/probe.log");
    }
    void openLog() {
        closeLog();
        const auto f = logPath(); f.getParentDirectory().createDirectory();
        log = std::fopen(f.getFullPathName().toRawUTF8(), "a");
        if (!log) return;
        std::fprintf(log, "# TidBank probe start %s sr=%.0f interval=%gs. t = host samples since prepareToPlay; delay/diff in samples; "
                          "marks at n0 = k*interval; 'blk a+len @off' = block start + length, @off = offset of the sample in it.\n",
                     juce::Time::getCurrentTime().toISO8601(true).toRawUTF8(), sampleRate, interval);
        std::fflush(log);
    }
    void closeLog() { if (log) { std::fclose(log); log = nullptr; } }
    void toLive(const Rec& r) {
        const uint32_t now = std::max<uint32_t>(1, juce::Time::getMillisecondCounter());
        if (r.type == kDelay) {
            const int k = r.engine == 0 ? r.head - 1 : 3;
            if (k < 0 || k > 3) return;
            live.measured[k] = r.measured; live.computed[k] = r.computed; live.seenMs[k] = now;
        } else if (r.type == kCpu) {
            live.cpuAvg = r.cpuAvg; live.cpuPeak = r.cpuPeak; live.drops = r.drops; live.cpuSeenMs = now;
        }
    }
    void drain() {
        char line[512]; Rec r; int lines = 0;
        while (hub.pop(r)) {
            toLive(r);
            if (log) { formatLine(r, sampleRate, line, sizeof line); std::fprintf(log, "%s\n", line); ++lines; }
        }
        if (!log) return;
        if (hub.drops() != reportedDrops) { reportedDrops = hub.drops(); std::fprintf(log, "# dropped records so far: %llu\n", (unsigned long long) reportedDrops); ++lines; }
        if (lines) std::fflush(log);
    }
    void timerCallback() override { const std::lock_guard<std::mutex> g(consumer); drain(); }

    std::mutex consumer;  // consumer side only (timer callback vs configure/destructor); never on the audio thread
    std::FILE* log = nullptr; double sampleRate = 48000.0, interval = 1.0; uint64_t reportedDrops = 0;
};

}}  // namespace tidbank::probe
#endif  // TIDBANK_DEBUG_PROBE
