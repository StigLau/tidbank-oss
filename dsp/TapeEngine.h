#pragma once
#include "TapeParams.h"
#include "TapeTransport.h"
#include "DelayLine.h"
#include "FeedbackNetwork.h"
#include <cmath>
#include <algorithm>
#ifdef TIDBANK_DEBUG_PROBE
#include "DebugProbe.h"
#endif
namespace tidbank {
// Mono tape-echo engine. Exact transport (D1'), three heads with sync/target/nudge (D2'), mode table. Feedback network (S3) at the record head.
// Header-only, JUCE-free, no allocation after prepare().
class TapeEngine {
public:
    static constexpr double kD1 = 0.5;                  // head 1 distance, tape-seconds at v = 1
    static constexpr double kVmin = kD1 / 2.0, kVmax = kD1 / 0.04;  // head 1 within 40 ms..2 s

    void prepare(double sampleRate, double maxDelaySec) {
        sr = sampleRate; maxDelay = maxDelaySec;
        tr.prepare(sr, maxDelay);
        net.prepare(sr);
        dl.prepare((size_t) std::ceil(maxDelay * sr) + 8);
        fresh = true;
    }
    // Next setParams() starts the tape at its target speed (no start-up glide).
    void reset() {  // no allocation: JUCE may call reset() on the audio thread
        dl.clear(); net.reset(); fresh = true;
#ifdef TIDBANK_DEBUG_PROBE
        if (tracer_) tracer_->clear();
#endif
    }
#ifdef TIDBANK_DEBUG_PROBE
    void attachProbe(probe::Tracer* t) { tracer_ = t; }  // observes tau/gain after the solve; never writes engine state
#endif
    void setParams(const TapeParams& p) {
        params = p;
        const double* r = ratios(p.spacing);
        for (int i = 0; i < 3; ++i) d[i] = r[i] * kD1 * sr;
        const double vt = targetSpeed(p);
        clamped = std::fabs(vt - unclampedSpeed(p)) > 1e-12;  // sync broken by the speed range: the host/UI should show it
        tr.setInertiaMs(p.inertia_ms);
        aD = std::exp(-6.638 / (std::max((double) p.inertia_ms, 1.0) * 1e-3 * sr));  // head moves glide like speed
        tr.setTarget(vt);
        net.setTargets(p.fb_hpf, p.fb_lpf, p.intensity, p.drive);
        for (int i = 0; i < 3; ++i) gainTarget[i] = active(i) ? 1.0 : 0.0;
        if (fresh) {
            tr.reset(vt);
            for (int i = 0; i < 3; ++i) { tau[i] = d[i] / vt; gain[i] = gainTarget[i]; dS1[i] = dS[i] = d[i]; }
            fresh = false;
        }
    }
    float process(float in) {
        tr.advance();
        const double step = 1.0 / (kFadeSec * sr);
        float y = 0.0f; double wSum = 0.0;
        for (int i = 0; i < 3; ++i) {
            dS1[i] = d[i] + aD * (dS1[i] - d[i]); dS[i] = dS1[i] + aD * (dS[i] - dS1[i]);  // spacing change: heads glide, no jump
            tau[i] = tr.solve(dS[i], tau[i]);  // solved every sample even when silent, so a head fades in in place
            gain[i] = gain[i] < gainTarget[i] ? std::min(gain[i] + step, gainTarget[i]) : std::max(gain[i] - step, gainTarget[i]);
            if (gain[i] > 0.0) {
                const double wi = std::sin(gain[i] * 1.5707963267948966);  // equal power
                y += (float) wi * dl.readSinc(tau[i] - kReadOffset);  // sinc: Hermite images at any ratio != 1
                wSum += wi;
            }
        }
#ifdef TIDBANK_DEBUG_PROBE
        if (tracer_) tracer_->tapeStep(tau, gain, tr.speed());
#endif
        // Feedback = head sum / total head weight, so loop gain <= intensity in every mode (intensity 1 = edge of
        // self-oscillation whatever the mode). Unnormalised, 3 heads ran away at intensity 0.34. Output y is not scaled.
        dl.push(net.record(in + net.loop(y / (float) std::max(wSum, 1.0))));
        return y;  // wet only
    }
    bool isClamped() const { return clamped; }
    // Internal state for ears: 0 = v, 1..3 = tau_i (samples), 4..6 = tau_i - d_i/v (samples, small values keep
    // float32 WAV precision; ~0 when settled), 7 = clamped flag.
    double probe(int which) const {
        if (which == 0) return tr.speed();
        if (which >= 1 && which <= 3) return tau[which - 1];
        if (which >= 4 && which <= 6) return tau[which - 4] - dS[which - 4] / tr.speed();
        return which == 7 ? (clamped ? 1.0 : 0.0) : 0.0;
    }

    // Target speed: the chosen head lands on the division (sync) or time_ms; nudge detunes the tempo.
    // Clamped after nudge: head 1 stays within 40 ms..2 s whichever head is targeted.
    static double targetSpeed(const TapeParams& p) { return std::min(std::max(unclampedSpeed(p), kVmin), kVmax); }
    static double unclampedSpeed(const TapeParams& p) {
        const int h = std::min(std::max(p.head_target, 1), 3) - 1;
        const double t = p.sync ? p.division * 60.0 / std::max(p.bpm, 1.0f) : p.time_ms * 1e-3;
        return ratios(p.spacing)[h] * kD1 / std::max(t, 1e-3) * std::exp2(p.nudge_cents / 1200.0);
    }
private:
    static const double* ratios(int spacing) {
        static const double even[3] = {1.0, 2.0, 3.0}, vintage[3] = {1.0, 1.9, 2.75};  // vintage PROVISIONAL (research item 1)
        return spacing == 1 ? vintage : even;
    }
    // RE-201 style mode selector -> head bitmask (bit i = head i+1). PROVISIONAL until research item 2:
    // 1-3 single heads, 4 = 2+3, 5-7 = 1+2, 1+3, 1+2+3, 8-11 repeat 1-4 (reverb variants on the RE-201,
    // no reverb here: D9), 12 = no heads (reverb only on the RE-201).
    static constexpr unsigned kModeHeads[12] = {0b001, 0b010, 0b100, 0b110, 0b011, 0b101, 0b111, 0b001, 0b010, 0b100, 0b110, 0b000};
    bool active(int i) const {
        if (params.head > 0) return params.head - 1 == i;  // solo (tests)
        return (kModeHeads[std::min(std::max(params.mode, 1), 12) - 1] >> i) & 1u;
    }
    static constexpr double kFadeSec = 0.020;
    // read-before-push (+1: the sample written this step is not in the buffer yet) and the ADAA
    // saturator's half-sample delay (+0.5): head i then sounds exactly tau_i after the input.
    static constexpr double kReadOffset = 1.5;  // head on/off crossfade on mode change

    TapeTransport tr; DelayLine dl; FeedbackNetwork net; TapeParams params;
    double sr = 48000.0, maxDelay = 8.0, d[3] = {0, 0, 0}, tau[3] = {0, 0, 0}, gain[3] = {0, 0, 0}, gainTarget[3] = {0, 0, 0},
           dS[3] = {0, 0, 0}, dS1[3] = {0, 0, 0}, aD = 0.0;
    bool fresh = true, clamped = false;
#ifdef TIDBANK_DEBUG_PROBE
    probe::Tracer* tracer_ = nullptr;
#endif
};
}
