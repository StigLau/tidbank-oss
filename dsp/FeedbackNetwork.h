#pragma once
#include <cmath>
#include <algorithm>
namespace tidbank {
// Feedback path of the tape engine (S3): loop() = HPF -> LPF -> intensity, record() = ADAA tanh saturator.
// Header-only, JUCE-free, no allocation. State is double. All parameters are smoothed per sample
// (one-pole, 20 ms time constant = 63 %); cutoffs in log-frequency, drive in dB.
class FeedbackNetwork {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        a = 1.0 - std::exp(-1.0 / (kSmoothSec * sr));
        reset();
    }
    // Clears filter and ADAA state; the next setTargets() snaps the smoothed parameters.
    void reset() {
        hp = Svf(); lp = Svf(); xPrev = 0.0;
        fresh = true;
    }
    // The first call after prepare()/reset() snaps (no ramp from 0); later calls glide.
    void setTargets(float hpfHz, float lpfHz, float intensity, float driveDb) {
        tHpf = std::log(clampHz(hpfHz)); tLpf = std::log(clampHz(lpfHz));
        tInt = intensity; tDrive = driveDb;
        if (fresh) { sHpf = tHpf; sLpf = tLpf; sInt = tInt; sDrive = tDrive; fresh = false; }
    }
    // headSum -> 2-pole Butterworth HPF -> 2-pole Butterworth LPF -> * intensity.
    float loop(float headSum) {
        sHpf = smooth(sHpf, tHpf); sLpf = smooth(sLpf, tLpf); sInt = smooth(sInt, tInt);
        const double x = headSum;
        const double h = hp.tick(x, std::tan(kPi * clampHz(std::exp(sHpf)) / sr)).hp;
        const double l = lp.tick(h, std::tan(kPi * clampHz(std::exp(sLpf)) / sr)).lp;
        return (float) (l * sInt);
    }
    // Saturator y = tanh(g x)/g, g = 10^(driveDb/20): unity small-signal gain, output bounded by 1/g.
    // First-order ADAA (antiderivative F(u) = logcosh(g u)/g^2) delays the signal by 0.5 sample
    // (a small signal sees (x + x_prev)/2); the engine compensates by reading heads at tau - 0.5.
    float record(float xin) {
        sDrive = smooth(sDrive, tDrive);
        const double g = std::exp(sDrive * (kLn10 / 20.0));
        const double x = xin, dx = x - xPrev;
        double y;
        if (std::fabs(dx) < kAdaaEps) y = std::tanh(g * 0.5 * (x + xPrev)) / g;
        else y = (antideriv(x, g) - antideriv(xPrev, g)) / dx;
        const double lim = 1.0 / g;  // rounding at huge |x| can exceed the true bound
        y = std::min(std::max(y, -lim), lim);
        xPrev = x;
        return (float) y;
    }

private:
    static constexpr double kPi = 3.14159265358979323846, kLn10 = 2.30258509299404568402;
    static constexpr double kSmoothSec = 0.020, kAdaaEps = 1e-5;
    // Denormal guard: below 1e-15 a decaying filter state is flushed to exactly 0. Otherwise the states
    // would decay into subnormal doubles (slow on x86, and the float output would turn subnormal),
    // and silence would never reach exact zeros.
    static constexpr double kFlush = 1e-15;

    struct Svf {  // TPT/ZDF state-variable filter (Zavalishin), Butterworth: k = 1/Q = sqrt 2
        double ic1 = 0.0, ic2 = 0.0;
        struct Out { double lp, hp; };
        Out tick(double v0, double g) {
            constexpr double k = 1.41421356237309504880;
            const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const double v3 = v0 - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
            if (std::fabs(ic1) < kFlush) ic1 = 0.0;
            if (std::fabs(ic2) < kFlush) ic2 = 0.0;
            return { v2, v0 - k * v1 - v2 };
        }
    };

    double clampHz(double f) const { return std::min(std::max(f, 10.0), 0.45 * sr); }
    // One-pole towards t; snaps once within 1e-9 so the state hits its target exactly (no subnormal tail).
    double smooth(double s, double t) const {
        s += a * (t - s);
        return std::fabs(t - s) < 1e-9 ? t : s;
    }
    static double logcosh(double z) {  // overflow-safe
        const double az = std::fabs(z);
        return az + std::log1p(std::exp(-2.0 * az)) - 0.69314718055994530942;
    }
    static double antideriv(double u, double g) { return logcosh(g * u) / (g * g); }

    double sr = 48000.0, a = 0.0;
    Svf hp, lp;
    double xPrev = 0.0;
    double tHpf = std::log(80.0), tLpf = std::log(6000.0), tInt = 0.0, tDrive = 0.0;
    double sHpf = tHpf, sLpf = tLpf, sInt = 0.0, sDrive = 0.0;
    bool fresh = true;
};
}  // namespace tidbank
