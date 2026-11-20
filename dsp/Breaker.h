#pragma once
// Output breaker: protects ears and speakers while testing. Trips at once on a non-finite sample, or when the
// output stays loud: a sample peak above +6 dBFS in every 10 ms window for 1 s (2026-11-16: "hitting 0 dB is not a
// problem", over +6 dB for more than 1 s is). Once tripped it ramps the output to silence in 1 ms and stays silent (latched)
// until it is switched off. Off = no protection, output passes. The plugin starts with it on every time (it is not saved
// with the project); switching it off and on again re-arms it.
// RT-safe: no allocation, no locks; `on` is written by the UI, `reason` read by the UI.
#include <algorithm>
#include <atomic>
#include <cmath>

namespace tidbank {

class Breaker {
public:
    enum Reason : int { kNone = 0, kNonFinite = 1, kLoud = 2 };
    static constexpr float kLoudLimit = 1.9952623f;  // +6 dBFS
    static constexpr double kWindowSec = 0.010, kFadeSec = 0.001;
    static constexpr int kHold = 100;  // consecutive 10 ms windows over the limit (1 s)

    std::atomic<bool> on {true};
    std::atomic<int> reason {kNone};

    static const char* describe(int r) {
        return r == kNonFinite ? "non-finite sample" : r == kLoud ? "over +6 dBFS for 1 s" : "";
    }

    void prepare(double sr) {
        win = std::max(1, (int) std::lround(sr * kWindowSec));
        fadeStep = 1.0f / (float) std::max(1, (int) std::lround(sr * kFadeSec));
        rearm();
    }

    // Audio thread, once per output sample (both channels). Returns the gain to apply: 1 until tripped, then a 1 ms ramp to 0.
    // At gain 0 the caller writes 0 (not x * 0, which stays NaN for a non-finite x).
    float step(float l, float r) {
        if (!on.load(std::memory_order_relaxed)) {
            if (!idle) { rearm(); idle = true; }  // off: pass through; the next switch-on starts from a clean detector
            return 1.0f;
        }
        idle = false;
        if (latched) {
            gain = std::max(0.0f, gain - fadeStep);
            if (gain == 0.0f && !erased) { erased = true; eraseDue = true; }
            return gain;
        }
        if (!std::isfinite(l) || !std::isfinite(r)) return trip(kNonFinite);
        over = over || std::max(std::abs(l), std::abs(r)) > kLoudLimit;
        if (++cnt == win) {  // window done: count it if any sample in it was over the limit, else start over
            loud = over ? loud + 1 : 0; over = false; cnt = 0;
            if (loud >= kHold) return trip(kLoud);
        }
        return 1.0f;
    }
    // Audio thread: true once, when the ramp has reached silence (the caller erases the echoes then).
    bool takeErase() { const bool e = eraseDue; eraseDue = false; return e; }
    bool tripped() const { return latched; }

private:
    float trip(int why) {  // non-finite: silent at once (a ramp cannot scale NaN); otherwise the 1 ms ramp starts here
        latched = true; reason.store(why, std::memory_order_relaxed);
        gain = why == kNonFinite ? 0.0f : std::max(0.0f, 1.0f - fadeStep);
        if (gain == 0.0f) { erased = true; eraseDue = true; }
        return gain;
    }
    void rearm() {
        latched = false; erased = false; eraseDue = false; gain = 1.0f;
        cnt = 0; loud = 0; over = false;
        reason.store(kNone, std::memory_order_relaxed);
    }

    int win = 480, cnt = 0, loud = 0;
    float gain = 1.0f, fadeStep = 1.0f / 48.0f;
    bool over = false, latched = false, erased = false, eraseDue = false, idle = false;
};

}  // namespace tidbank
