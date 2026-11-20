#pragma once
// Filtær v0 (d38): the Moog Matriarch's two-filter section as a stereo effect. JUCE-free, header-only.
//
// Facts it follows (docs/research/filter-matriarch.md section 1, matriarch-spacing-measurement.md Results):
// - two 4-pole (24 dB/oct) ladders. VCF 2 is always low-pass; VCF 1 is high-pass in SERIES and PARALLEL, low-pass in STEREO.
// - HP/LP SERIES: in -> VCF 1 (HP) -> VCF 2 (LP), mono. LP/LP STEREO: VCF 1 -> L, VCF 2 -> R, not summed.
//   HP/LP PARALLEL: VCF 1 (HP) + VCF 2 (LP), summed to mono. Both filters get the same (mono) signal, as on the hardware.
// - CUTOFF sets both; SPACING offsets VCF 1 only, in octaves, linear in the knob, about +-5.8 oct; 0 = unison (measured).
// - Separate resonances; self-oscillation from about 3 o'clock on the knob (MM p. 22) = 0.8 of its travel.
// - Bass loss against resonance, measured 2026-11-19 (65 Hz, cutoff ~2.4-2.7 kHz, per filter): about -11 dB at RES noon, -14 to -15 dB
//   at 3 o'clock; close to the linear ladder's 1/(1+k) (-10.9 dB at k 2.5, -14.0 dB at k 4), so there is no bass compensation.
//   Take 0017 (3.3 / 6.2 dB) is an unexplained outlier (open, unexplained), dropped 2026-11-20.
//
// Model: zero-delay-feedback (TPT) ladder of four identical one-poles, feedback solved linearly, then tanh on the summed input
// (u = tanh(x - k y4)). That bounds self-oscillation; the stages themselves are linear (per-stage tanh is a later step).
// HP is the binomial tap mix of the same core: u - 4 y1 + 6 y2 - 4 y3 + y4 = (1 - LP1)^4 u.
#include <algorithm>
#include <cmath>

namespace tidbank {

class Ladder {
public:
    struct Out { double lp, hp; };
    void reset() { for (double& v : s) v = 0.0; }
    // g = tan(pi fc / fs); k = resonance feedback (4 = edge of self-oscillation in the linear model).
    Out tick(double x, double g, double k) {
        const double G = g / (1.0 + g), a = 1.0 / (1.0 + g);
        const double G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const double sigma = G3 * s[0] * a + G2 * s[1] * a + G * s[2] * a + s[3] * a;
        const double y4 = (G4 * x + sigma) / (1.0 + k * G4);
        const double u = std::tanh(x - k * y4);
        double in = u, y[4];
        for (int i = 0; i < 4; ++i) {
            const double v = (in - s[i]) * G;
            y[i] = v + s[i];
            s[i] = y[i] + v;
            if (std::fabs(s[i]) < 1e-15) s[i] = 0.0;  // no subnormals (tidbank-rt)
            in = y[i];
        }
        return { y[3], u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3] };
    }
private:
    double s[4] = {0, 0, 0, 0};
};

struct FiltaerParams {
    enum Mode { Series = 0, Stereo = 1, Parallel = 2 };  // the Matriarch's FILTER MODE positions, in panel order
    int mode = Stereo;
    float cutoffHz = 2000.0f;  // both filters (VCF 2's cutoff)
    float spacingOct = 0.0f;   // VCF 1 against VCF 2, +-5.8
    float res1 = 0.0f, res2 = 0.0f;  // knob 0..1; self-oscillation from 0.8
    float driveDb = 0.0f;      // 0..24: input gain into the ladder's tanh; the output is divided by it (small signals keep level)
    float mix = 1.0f;          // 0 = dry (bit-exact), 1 = filtered
};

class Filtaer {
public:
    // SPACING limit: a symmetric approximation of the measured +5.91 / -5.63 oct (negative side 0.17 oct past the hardware)
    static constexpr double kSpacingMaxOct = 5.8, kMinHz = 10.0, kMaxFsRatio = 0.45, kSmoothSec = 0.02, kModeFadeSec = 0.005;
    static constexpr double kSelfOscKnob = 0.8, kResK = 4.0 / kSelfOscKnob;  // knob 0.8 -> k 4, knob 1 -> k 5
    static constexpr double kNoise = 1e-9, kPi = 3.14159265358979323846;  // not M_PI: not standard C++ (MSVC)

    static double resK(double knob) { return kResK * std::min(std::max(knob, 0.0), 1.0); }

    void prepare(double sampleRate) { fs = sampleRate; reset(); first = true; }
    void reset() { f1.reset(); f2.reset(); }

    void setParams(const FiltaerParams& p) {
        tgt = p;
        tgt.spacingOct = (float) std::min(std::max((double) p.spacingOct, -kSpacingMaxOct), kSpacingMaxOct);
        if (first) {  // the first setParams after prepare snaps
            first = false;
            lc = std::log2(std::max((double) tgt.cutoffHz, kMinHz)); sp = tgt.spacingOct;
            r1 = tgt.res1; r2 = tgt.res2; dr = tgt.driveDb; mx = tgt.mix; mode = tgt.mode; fade = 1.0;
        }
    }

    // One stereo frame. Dry passes in stereo; both filters see the mono sum.
    void process(const float in[2], float out[2]) {
        const double a = 1.0 - std::exp(-1.0 / (kSmoothSec * fs));
        lc += a * (std::log2(std::max((double) tgt.cutoffHz, kMinHz)) - lc);
        sp += a * (tgt.spacingOct - sp);
        r1 += a * (tgt.res1 - r1); r2 += a * (tgt.res2 - r2);
        dr += a * (tgt.driveDb - dr); mx += a * (tgt.mix - mx);
        if (std::fabs(tgt.mix - mx) < 1e-7) mx = tgt.mix;  // mix 0 must reach exactly 0: dry bit-exact
        // A mode change ducks the wet path to 0, switches with cleared filters, and comes back (no click).
        const double step = 1.0 / (kModeFadeSec * fs);
        if (tgt.mode != mode) { fade -= step; if (fade <= 0.0) { fade = 0.0; mode = tgt.mode; reset(); } }
        else if (fade < 1.0) fade = std::min(1.0, fade + step);

        const double hi = kMaxFsRatio * fs;
        const double fc2 = std::min(std::max(std::exp2(lc), kMinHz), hi), fc1 = std::min(std::max(std::exp2(lc + sp), kMinHz), hi);
        const double g1 = std::tan(kPi * fc1 / fs), g2 = std::tan(kPi * fc2 / fs);
        const double k1 = resK(r1), k2 = resK(r2);
        const double gd = std::pow(10.0, dr / 20.0);
        // + a noise floor at -180 dBFS, as in the analog circuit: the filters self-oscillate from silence like the Matriarch's do
        seed = seed * 1664525u + 1013904223u;
        const double m = 0.5 * ((double) in[0] + (double) in[1]) * gd + kNoise * ((double) (seed >> 8) / 8388608.0 - 1.0);
        double wl, wr;
        if (mode == FiltaerParams::Stereo) {
            wl = f1.tick(m, g1, k1).lp;
            wr = f2.tick(m, g2, k2).lp;
        } else if (mode == FiltaerParams::Series) {
            wl = wr = f2.tick(f1.tick(m, g1, k1).hp, g2, k2).lp;
        } else {
            wl = wr = f1.tick(m, g1, k1).hp + f2.tick(m, g2, k2).lp;
        }
        const double w = fade / gd;
        out[0] = (float) (in[0] + mx * (wl * w - in[0]));
        out[1] = (float) (in[1] + mx * (wr * w - in[1]));
    }

    double cutoff1Hz() const { return std::min(std::max(std::exp2(lc + sp), kMinHz), kMaxFsRatio * fs); }  // after the clamp
    double cutoff2Hz() const { return std::min(std::max(std::exp2(lc), kMinHz), kMaxFsRatio * fs); }
    int activeMode() const { return mode; }

private:
    double fs = 48000.0;
    FiltaerParams tgt;
    bool first = true;
    double lc = 11.0, sp = 0.0, r1 = 0.0, r2 = 0.0, dr = 0.0, mx = 1.0, fade = 1.0;
    int mode = FiltaerParams::Stereo;
    unsigned seed = 1;
    Ladder f1, f2;  // VCF 1, VCF 2
};

}  // namespace tidbank
