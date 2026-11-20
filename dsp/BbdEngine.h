#pragma once
#include "BbdProfile.h"
#include "BbdFilter.h"
#include "Compander.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#ifdef TIDBANK_DEBUG_PROBE
#include "DebugProbe.h"
#endif
namespace tidbank {
// BBD performance parameters (fixed host parameters, d21: a profile changes what they mean, not which exist).
struct BbdParams {
    float time_ms = 300.0f;  // delay, clamped to the profile's TIME range (spacedMaxMs on the spaced line)
    int spaced = 0;          // 1 = this is the spaced line (Delay 1): it may run past TIME max, up to spacedMaxMs (d32)
    float feedback = 0.0f;   // 0..1.4 = Intensity, scaled by loopNorm; taken at the expander output, summed before the
                             // compressor. Above the runaway point the loop runs away by design.
    int droopComp = 0;       // 0 = authentic (d36): the output hold droops the loop more at long delays, so the runaway point
                             // moves with the delay (d4096 Intensity 1.01 at 20 ms, 1.31 at 300 ms). 1 = compensated
                             // (loopDroopScale): Intensity = peak loop gain, runaway at 1, at every delay.
    int bright = 1;          // 1 = Bright (filter set 0), 0 = Dark (set 1, if the profile has one)
    int compander = 1;       // 0 bypasses compressor and expander (tests: filter-only response)
    int noise = 1;           // 0 disables BBD noise (tests)
    int nonlin = 1;          // 0 disables the BBD nonlinearity (tests: LTI response)
};

// Mono BBD echo, "simple" model path (d18). Wet output only. Header-only, JUCE-free, no allocation after prepare().
// Feedback is normalised per profile: fb x loopNorm() makes Intensity the peak loop gain of the filters in every profile;
// droopComp also undoes the output hold per clock (d36).
// Chain per host sample: s = in + fb * norm * yPrev -> compressor -> anti-alias (host rate) -> BBD (clock-tick sampled
// ring of N/2 cells, f_clk = N / (2 t), linear interpolation of the input at the tick time, RS10 nonlinearity, chip noise)
// -> reconstruction filter on the zero-order-held BBD output (aliasing of the hold is accepted here) -> 20 Hz DC blocker
// (coupling cap; the nonlinearity's even term makes DC) -> expander. yPrev = expander output.
// Bright/Dark: both filter sets always run; the set weight glides over 20 ms (no click on a switch).
// The circuit-level paths (Holters & Parker, BLEP) come later; this one trades fidelity for cost and simplicity.
class BbdEngine {
public:
    void prepare(double sampleRate, const BbdProfile& profile) {
        sr = sampleRate; prof = profile;
        for (int k = 0; k < 2; ++k) { aa[k].design(prof.aa[k], sr); recon[k].design(prof.recon[k], sr); }
        comp.prepare(sr, prof.compTauMs.v, prof.compRef.v);
        normComp = tidbank::loopNorm(prof, sr, true); normBypass = tidbank::loopNorm(prof, sr, false);
        logClkLo = std::log(prof.stages.v / (2.0 * std::max(prof.spacedMaxMs.v, prof.delayMaxMs.v) * 1e-3));
        logClkHi = std::log(prof.stages.v / (2.0 * prof.delayMinMs.v * 1e-3));
        for (int k = 0; k < 2; ++k) {
            droop[k].assign((size_t) kDroopN, 1.0);
            tidbank::loopDroopScale(prof, sr, std::exp(logClkLo), std::exp(logClkHi), k == 0, droop[k].data(), kDroopN);
        }
        cells = std::max(1, (int) std::lround(prof.stages.v * 0.5));
        ring.assign((size_t) cells, 0.0);
        smoothA = 1.0 - std::exp(-1.0 / (kSmoothSec * sr));
        dcA = std::exp(-2.0 * kPi * 20.0 / sr);
        noiseScale = prof.fullScale.v / std::sqrt(2.0) * std::pow(10.0, -prof.snrDb.v / 20.0) * std::sqrt(3.0);
        reset();
    }
    // Clears ring, filters, compander, dc blocker and clock phase in place; the next setParams() snaps the clock.
    void reset() {
        std::fill(ring.begin(), ring.end(), 0.0);
        for (int k = 0; k < 2; ++k) { aa[k].reset(); recon[k].reset(); }
        comp.reset();
        pos = 0; phase = 0.0; aPrev = 0.0; held = 0.0; yPrev = 0.0; dcX = dcY = 0.0;
        rng = kSeed; fresh = true;
#ifdef TIDBANK_DEBUG_PROBE
        if (tracer_) tracer_->clear();
#endif
    }
#ifdef TIDBANK_DEBUG_PROBE
    void attachProbe(probe::Tracer* t) { tracer_ = t; }  // observes clock ticks; never writes engine state
#endif
    void setParams(const BbdParams& p) {
        params = p;
        const double t = std::min(std::max((double) p.time_ms, prof.delayMinMs.v), maxMs(p));
        tLogClk = std::log(prof.stages.v / (2.0 * t * 1e-3));
        if (fresh) { sLogClk = tLogClk; gDark = (p.bright || !prof.hasDark) ? 0.0 : 1.0; fresh = false; }
    }
    float process(float in) { return process(in, yPrev); }
    // fbSource: what the feedback taps (normally this engine's own last output). Ping-pong feeds the other line's
    // lastOut() instead, so the echo crosses L -> R -> L (Matriarch PING PONG). Loop gain per round trip <= Intensity^2.
    float process(float in, double fbSource) {
        sLogClk += smoothA * (tLogClk - sLogClk);
        if (std::fabs(tLogClk - sLogClk) < 1e-9) sLogClk = tLogClk;
        fclk = std::exp(sLogClk);
        const double step = fclk / sr;
#ifdef TIDBANK_DEBUG_PROBE
        if (tracer_) tracer_->bbdBegin();
#endif

        scaleNow = params.droopComp ? droopAt(params.compander ? 0 : 1) : (params.compander ? normComp : normBypass);
        const double fbk = params.feedback * scaleNow;
        const double s = in + fbk * fbSource;
        const double c = params.compander ? comp.compress(s) : s;
        const double gT = (params.bright || !prof.hasDark) ? 0.0 : 1.0;  // weight of the Dark set
        gDark += smoothA * (gT - gDark); if (std::fabs(gT - gDark) < 1e-9) gDark = gT;
        const double a0 = aa[0].tick(c), a1 = prof.hasDark ? aa[1].tick(c) : a0;
        const double a = a0 + gDark * (a1 - a0);

        phase += step;
        while (phase >= 1.0) {
            phase -= 1.0;
            const double u = 1.0 - phase / step;  // tick time inside this sample, 0 = previous sample, 1 = now
#ifdef TIDBANK_DEBUG_PROBE
            if (tracer_) tracer_->bbdTick((double) (tracer_->now() - 1) + u, cells, pos, fclk, probe(1));
#endif
            double v = (aPrev + (a - aPrev) * u) / prof.fullScale.v;
            if (params.nonlin) v = nonlin(v);
            const double old = ring[(size_t) pos]; ring[(size_t) pos] = v;
            if (++pos >= cells) pos = 0;
            held = old * prof.fullScale.v;
            if (params.noise) held += noiseScale * nextNoise();
        }
        aPrev = a;

        const double r0 = recon[0].tick(held), r1 = prof.hasDark ? recon[1].tick(held) : r0;
        const double r = r0 + gDark * (r1 - r0);
        dcY = dcA * (dcY + r - dcX); dcX = r;
        if (std::fabs(dcY) < kFlush) dcY = 0.0;
        const double y = params.compander ? comp.expand(dcY) : dcY;
        yPrev = std::fabs(y) < kFlush ? 0.0 : y;
        return (float) y;
    }
    double lastOut() const { return yPrev; }  // the last output (expander), the feedback tap
    double maxMs(const BbdParams& p) const { return p.spaced ? prof.spacedMaxMs.v : prof.delayMaxMs.v; }
    // Feedback scale in use (1 = the chain has no gain peak); compander on / bypassed.
    double loopScale(bool compander = true) const { return compander ? normComp : normBypass; }
    double fbScale() const { return scaleNow; }  // the feedback scale the last sample used (droopComp: at that clock)
    // 0 = current clock (Hz), 1 = current delay N / (2 f_clk) (ms), 2 = compressor rectifier, 3 = expander rectifier
    double probe(int i) const {
        switch (i) {
            case 0: return fclk;
            case 1: return fclk > 0.0 ? prof.stages.v / (2.0 * fclk) * 1e3 : 0.0;
            case 2: return comp.compAvg();
            case 3: return comp.expAvg();
        }
        return 0.0;
    }
private:
    static constexpr double kPi = 3.14159265358979323846, kSmoothSec = 0.020, kFlush = 1e-15;
    static constexpr uint32_t kSeed = 0x1234567u;
    // RS10 f(x) = x - a x^2 - b x^3 inside |x| < 1, constant outside. bbd-research 5.1 transcribes the polynomial with a
    // trailing "+ a"; it is dropped here: with it the clip branches (1 - a - b, -1 - a + b) would not join the polynomial
    // at |x| = 1, and it is only a constant offset (the DC blocker removes the remaining even-term DC).
    double nonlin(double x) const {
        const double a = prof.nlA.v, b = prof.nlB.v;
        if (x >= 1.0) return 1.0 - a - b;
        if (x <= -1.0) return -1.0 - a + b;
        return x - a * x * x - b * x * x * x;
    }
    // Uniform in [-1, 1) from a deterministic LCG (Numerical Recipes), unit RMS after the sqrt(3) in noiseScale.
    double nextNoise() { rng = rng * 1664525u + 1013904223u; return (double) (int32_t) rng * (1.0 / 2147483648.0); }

    double sr = 48000.0; BbdProfile prof{}; BbdParams params;
    SosCascade aa[2], recon[2];
    Compander comp;
    std::vector<double> ring;
    int cells = 1, pos = 0;
    double smoothA = 0.0, dcA = 0.0, noiseScale = 0.0, normComp = 1.0, normBypass = 1.0, scaleNow = 1.0;
    static constexpr int kDroopN = 64;
    std::vector<double> droop[2];  // loopDroopScale per log clock over [logClkLo, logClkHi]: [0] compander on, [1] bypassed
    double logClkLo = 0.0, logClkHi = 1.0;
    double droopAt(int k) const {
        const double u = std::min(std::max((sLogClk - logClkLo) / (logClkHi - logClkLo), 0.0), 1.0) * (kDroopN - 1);
        const int i = std::min((int) u, kDroopN - 2); const double t = u - i;
        return droop[k][(size_t) i] + t * (droop[k][(size_t) i + 1] - droop[k][(size_t) i]);
    }
    double tLogClk = 0.0, sLogClk = 0.0, fclk = 0.0, phase = 0.0;
    double aPrev = 0.0, held = 0.0, yPrev = 0.0, dcX = 0.0, dcY = 0.0, gDark = 0.0;
    uint32_t rng = kSeed;
    bool fresh = true;
#ifdef TIDBANK_DEBUG_PROBE
    probe::Tracer* tracer_ = nullptr;
#endif
};
}  // namespace tidbank
