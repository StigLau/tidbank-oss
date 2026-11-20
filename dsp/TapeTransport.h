#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
namespace tidbank {
// Tape speed with inertia and tape position, exact head delays (decision D1', d13).
// Units: time in samples; X in "tape samples" (X advances by v per sample, v = 1 nominal).
// A head d tape-samples from the record head has delay tau solving X(n) - X(n - tau) = d.
// Pitch of a repeat is v(n) / v(n - tau): bounded by the speed ratio, never reverses.
class TapeTransport {
public:
    void prepare(double sampleRate, double maxDelaySec) {
        sr = sampleRate;
        size = (size_t) std::ceil(maxDelaySec * sr) + 8;
        X.assign(size, 0.0); V.assign(size, 1.0);
        reset(1.0);
    }
    // History as if the tape had always run at v0 (no "buffer not full" special case).
    void reset(double v0) {
        v = v1 = target = v0; w = 0;
        for (size_t j = 0; j < size; ++j) { const size_t k = (size - j) % size; X[k] = -v0 * (double) j; V[k] = v0; }
    }
    // inertiaMs = time to 99 % of a step (two cascaded one-poles, critically damped: x = 6.638).
    void setInertiaMs(double ms) { a = std::exp(-6.638 / (std::max(ms, 1.0) * 1e-3 * sr)); }
    void setTarget(double vTarget) { target = vTarget; }
    // Call once per sample before reading heads.
    void advance() {
        v1 = target + a * (v1 - target);
        v = v1 + a * (v - v1);
        const double xPrev = X[w];
        w = (w + 1) % size;
        V[w] = v; X[w] = xPrev + v;
    }
    // One Newton step on tau (samples) for a head d tape-samples away; pass last sample's tau.
    // tau may grow by at most 1 per sample (pitch >= 0: tape never plays backwards). This only
    // binds when one Newton step overshoots (inertia far below 30 ms with big jumps).
    double solve(double d, double tauPrev) const {
        const double f = X[w] - at(X, tauPrev) - d;
        const double tau = std::min(tauPrev - f / std::max(at(V, tauPrev), 1e-6), tauPrev + 1.0);
        return std::min(std::max(tau, 2.0), (double) size - 4.0);
    }
    double speed() const { return v; }
    double sampleRate() const { return sr; }
private:
    // Linear interpolation of a ring at "tau samples before now".
    double at(const std::vector<double>& r, double tau) const {
        const double pos = (double) w - tau;
        const double fl = std::floor(pos), fr = pos - fl;
        long i = (long) fl % (long) size; if (i < 0) i += (long) size;
        const size_t i0 = (size_t) i, i1 = (i0 + 1) % size;
        return r[i0] + fr * (r[i1] - r[i0]);
    }
    std::vector<double> X, V;
    size_t size = 0, w = 0;
    double sr = 48000.0, v = 1.0, v1 = 1.0, target = 1.0, a = 0.0;
};
}
