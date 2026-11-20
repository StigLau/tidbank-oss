#pragma once
#include "BbdFilter.h"
#include <cmath>
#include <vector>
#include <cstring>
namespace tidbank {
// BBD profiles (d21): fixed wiring in BbdEngine, the numbers here. Every number carries its status and source
// (docs/research/bbd-research.md sections). Verified = read in an opened source; Measured = read from bench recordings of
// one real unit (bbd-research 8.1b); Inferred = follows from sources but not stated for this unit; Placeholder = no
// source, a stand-in until measured or found.
// Generic ids, no trademarks (d21). "basis" names what the numbers come from, for us, not for the UI.
enum class Src { Verified, Measured, Inferred, Placeholder };
inline const char* srcName(Src s) {
    return s == Src::Verified ? "verified" : s == Src::Measured ? "measured" : s == Src::Inferred ? "inferred" : "placeholder";
}

struct Fact { double v; Src s; const char* ref; };

struct BbdProfile {
    const char* id; const char* basis;
    Fact stages;                       // N; t_d = N / (2 f_clk)
    Fact delayMinMs, delayMaxMs;       // the unit's TIME range
    Fact spacedMaxMs;                  // longest delay of the spaced line (Delay 1): TIME max + SPACING max; >= delayMaxMs
    Fact chipClkMinHz, chipClkMaxHz;   // datasheet clock range of the chip family
    bool overclock, underclock;        // the TIME range runs the clock past the datasheet (declared, never clamped)
    Fact snrDb;                        // chip S/N (A-weighted, datasheet), sets the BBD noise floor re full scale
    bool hasDark;                      // two filter sets: [0] Bright / only, [1] Dark
    FilterDesign aa[2], recon[2];
    Src filterSrc; const char* filterRef;
    Fact wetBwHz[2];                   // target: -3 dB of the wet path for a steady sine with the compander on (0 = none)
    bool compander;
    Fact compTauMs;                    // 10 kOhm x C_RECT
    Fact compRef;                      // digital level of compander unity gain
    Fact nlA, nlB;                     // RS10 nonlinearity f(x) = x - a x^2 - b x^3, clipped outside |x| < 1
    Fact fullScale;                    // digital level at the BBD input that maps to RS10 x = 1
    Src fbSrc; const char* fbRef;      // feedback taken at the expander output, re-enters before the compressor
};

inline double clockHz(const BbdProfile& p, double delayMs) { return p.stages.v / (2.0 * delayMs * 1e-3); }

// Per filter in a 2-filter chain, through a 2:1/1:2 compander (dB doubled): each Butterworth filter of `order` sits at
// -3/4 dB at the wet target. ASSUMPTION: the published wet bandwidth was measured with a steady sine through the
// compander (Fable review of matriarch-measurement.md, finding 3). Without the compander it would be -1.5 dB each.
inline double fcForWetTarget(double targetHz, int order) {
    return targetHz / std::pow(std::pow(10.0, 0.75 / 10.0) - 1.0, 1.0 / (2.0 * order));
}

namespace bbd_detail {
inline FilterDesign rs10Aa() { FilterDesign d; d.add3(6.8e-9, 82e-9, 330e-12); return d; }
inline FilterDesign rs10Recon() { FilterDesign d; d.add3(2.2e-9, 33e-9, 1e-9).add2(39e-9, 330e-12); return d; }
constexpr Src V = Src::Verified, M = Src::Measured, I = Src::Inferred, P = Src::Placeholder;
}

// m8192: Matriarch delay line (= Moog 500 Series Analog Delay, Matriarch manual p. 32).
inline BbdProfile profileM8192() {
    using namespace bbd_detail;
    BbdProfile p{};
    p.id = "m8192"; p.basis = "Moog Matriarch delay / Moog 500 Series Analog Delay";
    p.stages = {8192, I, "500 manual pp. 15-16 (8192 buckets); Matriarch = 500 delay (Matriarch manual p. 32); 8.1"};
    // d32 (2026-11-18): the measured TIME range; the manual says 35-780 ms (Matriarch manual p. 32, 8.1).
    p.delayMinMs = {41.65, M, "knob fully ccw, 6 notes 41.63-41.69 ms; 8.1b"};
    p.delayMaxMs = {404.18, M, "knob fully cw, 15 notes 404.17-404.26 ms; 8.1b"};
    p.spacedMaxMs = {764, I, "TIME max 404.18 + SPACING max +360.2 ms (measured at TIME 203); sum not measured; 8.1b"};
    p.chipClkMinHz = {10e3, I, "DS3008 (verified, 7); MN3008 itself is an inference (4 x 2048, 8.1)"};
    p.chipClkMaxHz = {100e3, I, "DS3008 (verified, 7); chip inferred"};
    p.overclock = false; p.underclock = false;  // 41.65 ms -> 98.3 kHz, 404.18 ms -> 10.13 kHz; Delay 1 at 764 ms -> 5.36 kHz
    p.snrDb = {78, I, "DS3008 S/N 78 dB, chip assumed; 7"};
    p.hasDark = true;
    p.filterSrc = I; p.filterRef = "6th-order in and out (500 manual Fig. 1, verified); Butterworth shape is a placeholder; corners measured (8.1b); measured slope about -50 dB/oct";
    p.wetBwHz[0] = {2350, M, "Bright -3 dB at TIME 202.9 ms (2.31 at 43.6, 2.09 at 403.6 ms), noise, compander on; 8.1b (d33)"};
    p.wetBwHz[1] = {1410, M, "Dark -3 dB at TIME 203 ms only; 8.1b (d33). The 500 manual gives 2.6 / 1.65 kHz (8.1)"};
    const double fb = fcForWetTarget(p.wetBwHz[0].v, 6), fd = fcForWetTarget(p.wetBwHz[1].v, 6);
    p.aa[0] = p.recon[0] = FilterDesign::butterworth(6, fb);
    p.aa[1] = p.recon[1] = FilterDesign::butterworth(6, fd);
    p.compander = true;
    p.compTauMs = {2.2, P, "unknown; DM-2 value (8.3) as stand-in until measured"};
    p.compRef = {0.25, P, "no source"};
    p.nlA = {1.0 / 8, P, "RS10 fit (generic BBD), 5.1"};
    p.nlB = {1.0 / 18, P, "RS10 fit (generic BBD), 5.1"};
    p.fullScale = {1.0, P, "no source"};
    p.fbSrc = I; p.fbRef = "500 manual Fig. 1 p. 16: expander out -> sum before compressor (verified for the 500); Matriarch = 500 (inference); 8.1";
    return p;
}

// d4096: Boss DM-2 (MN3005, NE571). Filter values = RS10 typical echo circuit = DM-2 clone BOM (section 7).
inline BbdProfile profileD4096() {
    using namespace bbd_detail;
    BbdProfile p{};
    p.id = "d4096"; p.basis = "Boss DM-2";
    p.stages = {4096, V, "MN3005 (Aion Amethyst DM-2 clone docs); 7"};
    p.delayMinMs = {20, V, "KIM; 3.2"};
    p.delayMaxMs = {300, V, "KIM; 3.2 (calibration 6.8 kHz at max = 301 ms, 7)"};
    p.spacedMaxMs = {300, P, "no spacing on the DM-2: TIME max"};
    p.chipClkMinHz = {10e3, V, "DS3005; 2.1"};
    p.chipClkMaxHz = {100e3, V, "DS3005; 2.1"};
    p.overclock = true; p.underclock = true;  // 20 ms -> 102.4 kHz, 300 ms -> 6.83 kHz
    p.snrDb = {75, V, "DS3005; 2.1"};
    p.hasDark = false;
    p.aa[0] = rs10Aa(); p.recon[0] = rs10Recon(); p.aa[1] = p.aa[0]; p.recon[1] = p.recon[0];
    // OPEN: this chain peaks +11.6 dB near 2 kHz (x2 in dB through the compander). Pre-/de-emphasis (8.2) or other R values
    // in the real DM-2 are not modelled; check the DM-2 schematic before trusting this profile's tone. The feedback range is
    // normalised (loopNorm): Intensity = peak loop gain; the wet output still carries the +23.6 dB (|H|^2) forward peak.
    p.filterSrc = I; p.filterRef = "cap values verified (7), RS10 topology with corrected 3rd-order a1 (BbdFilter.h); emphasis not modelled";
    p.wetBwHz[0] = {0, P, "none published"}; p.wetBwHz[1] = p.wetBwHz[0];
    p.compander = true;
    p.compTauMs = {2.2, V, "C9/C25 220 nF x 10 kOhm (clone BOM); 8.3"};
    p.compRef = {0.25, P, "no source"};
    p.nlA = {1.0 / 8, P, "RS10 fit (generic BBD), 5.1; that RS10 modelled a DM-2 is itself an inference (7)"};
    p.nlB = {1.0 / 18, P, "RS10 fit (generic BBD), 5.1"};
    p.fullScale = {1.0, P, "no source"};
    p.fbSrc = I; p.fbRef = "generic Boss BBD block diagram: fed back into the compressor; 8.3";
    return p;
}

// c8192: MXR Carbon Copy (4 x BL3208, SA571, DM-2 filters).
inline BbdProfile profileC8192() {
    using namespace bbd_detail;
    BbdProfile p{};
    p.id = "c8192"; p.basis = "MXR Carbon Copy";
    p.stages = {8192, V, "traced schematic: 4 x BL3208A; 8.2"};
    p.delayMaxMs = {600, V, "\"Up to 600 ms\" (DUNLOP product page); 3.2"};
    p.spacedMaxMs = {600, P, "no spacing on the Carbon Copy: TIME max"};
    p.chipClkMinHz = {10e3, I, "DS3208 (BL3208 = MN3208 clone, assumed); 2.1"};
    p.chipClkMaxHz = {100e3, I, "DS3208; 2.1"};
    p.delayMinMs = {8192 / (2.0 * 100e3) * 1e3, P, "unknown; set to the 100 kHz chip limit (40.96 ms)"};
    p.overclock = false; p.underclock = true;  // 600 ms -> 6.83 kHz
    p.snrDb = {71, I, "DS3208 S/N 71 dB; 2.1"};
    p.hasDark = false;
    p.aa[0] = rs10Aa(); p.recon[0] = rs10Recon(); p.aa[1] = p.aa[0]; p.recon[1] = p.recon[0];
    p.filterSrc = I; p.filterRef = "\"exactly the same\" filters as the DM-2 (schematic author, 8.2); DM-2 values inferred";
    p.wetBwHz[0] = {0, P, "none published"}; p.wetBwHz[1] = p.wetBwHz[0];
    p.compander = true;
    p.compTauMs = {2.2, P, "unknown; DM-2 value (8.3)"};
    p.compRef = {0.25, P, "no source"};
    p.nlA = {1.0 / 8, P, "RS10 fit (generic BBD), 5.1"};
    p.nlB = {1.0 / 18, P, "RS10 fit (generic BBD), 5.1"};
    p.fullScale = {1.0, P, "no source"};
    p.fbSrc = P; p.fbRef = "assumed as DM-2";
    return p;
}

// Peak over frequency of the loop's filter gain at host rate fs: max_f [max_s |AA_s(f)| x max_s |RECON_s(f)|], a bound for
// any Bright/Dark blend (the engine mixes the two sets' outputs). The rest of the loop (BBD hold and input interpolation,
// DC blocker) is <= 1 and left out, so the bound is conservative. 5 Hz grid to fs/2, then 0.05 Hz around the best point.
// Called from prepare() only (about 20k |H| evaluations per filter pair).
inline double loopPeakGain(const BbdProfile& p, double fs) {
    SosCascade a[2], r[2];
    const int ns = p.hasDark ? 2 : 1;
    for (int s = 0; s < ns; ++s) { a[s].design(p.aa[s], fs); r[s].design(p.recon[s], fs); }
    auto g = [&](double f) {
        double ma = -1e300, mr = -1e300;
        for (int s = 0; s < ns; ++s) { ma = std::fmax(ma, a[s].magDb(f, fs)); mr = std::fmax(mr, r[s].magDb(f, fs)); }
        return ma + mr;
    };
    double best = g(0.0), fBest = 0.0;
    for (double f = 5.0; f < 0.5 * fs; f += 5.0) { const double v = g(f); if (v > best) { best = v; fBest = f; } }
    for (double f = std::fmax(0.0, fBest - 5.0); f <= fBest + 5.0; f += 0.05) best = std::fmax(best, g(f));
    return std::pow(10.0, best / 20.0);
}
// Feedback scale so that Intensity is the peak loop gain in every profile (2026-11-16 "normalise per profile").
// Through the 2:1 / 1:2 compander a filter between compressor and expander counts twice (Compander.h), so the steady loop
// gain is fb x |H|^2; with the compander bypassed (tests) it is fb x |H|. A chain that never exceeds unity (m8192) gets
// exactly 1: values within 1e-9 of unity are rounding, not gain.
inline double loopNorm(const BbdProfile& p, double fs, bool compander) {
    const double g = loopPeakGain(p, fs), L = compander ? g * g : g;
    return L > 1.0 + 1e-9 ? 1.0 / L : 1.0;
}

// d36 option 1: a feedback scale that also undoes the BBD output hold, sinc(f / f_clk) (Holters & Parker eq. 34), so
// Intensity is the peak loop gain at every delay, within ~0.4 % (measured 1.004 at the lowest clock: hold images the model
// leaves out, not proven). scale(f_clk) = 1 / max_f [|AA x RECON|(f) x sinc(f / f_clk)]^k, k = 2 with
// the compander (it doubles a filter's dB), 1 bypassed. One 10 Hz grid of |H| (same Bright/Dark bound as loopPeakGain), then
// n clock points log-spaced over [clkLo, clkHi] into out[0..n-1]. Called from prepare() only. Without it the hold moves the
// runaway point with the delay (d4096 Intensity 1.01 at 20 ms, 1.31 at 300 ms; m8192 1.00 to 1.02).
inline void loopDroopScale(const BbdProfile& p, double fs, double clkLo, double clkHi, bool compander, double* out, int n) {
    SosCascade a[2], r[2];
    const int ns = p.hasDark ? 2 : 1;
    for (int s = 0; s < ns; ++s) { a[s].design(p.aa[s], fs); r[s].design(p.recon[s], fs); }
    const int nf = (int) (0.5 * fs / 10.0);
    std::vector<double> h((size_t) nf);
    for (int j = 0; j < nf; ++j) {
        const double f = 10.0 * j; double ma = -1e300, mr = -1e300;
        for (int s = 0; s < ns; ++s) { ma = std::fmax(ma, a[s].magDb(f, fs)); mr = std::fmax(mr, r[s].magDb(f, fs)); }
        h[(size_t) j] = std::pow(10.0, (ma + mr) / 20.0);
    }
    for (int i = 0; i < n; ++i) {
        const double clk = clkLo * std::pow(clkHi / clkLo, n > 1 ? (double) i / (n - 1) : 0.0);
        double peak = 0.0;
        for (int j = 0; j < nf; ++j) {
            const double x = 3.14159265358979323846 * 10.0 * j / clk;
            peak = std::fmax(peak, h[(size_t) j] * (x > 0.0 ? std::fabs(std::sin(x) / x) : 1.0));
        }
        out[i] = 1.0 / (compander ? peak * peak : peak);
    }
}

inline int profileCount() { return 3; }
inline BbdProfile profileAt(int i) { return i == 0 ? profileM8192() : i == 1 ? profileD4096() : profileC8192(); }
inline bool findProfile(const char* id, BbdProfile& out) {
    for (int i = 0; i < profileCount(); ++i) { BbdProfile p = profileAt(i); if (!std::strcmp(p.id, id)) { out = p; return true; } }
    return false;
}
}  // namespace tidbank
