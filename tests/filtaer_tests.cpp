// Filtær v0 core (dsp/Filtaer.h, d38): ladder response, self-oscillation, SPACING, the three Matriarch routings,
// bass loss against the Matriarch's 2026-11-19 readings (linear ladder), dry bit-exact at mix 0, mode change without a click, allocation. JUCE-free.
#include "Filtaer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <utility>
#include <vector>

static long allocs = 0;
static bool counting = false;
void* operator new(std::size_t n) { if (counting) ++allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (counting) ++allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using tidbank::Filtaer; using tidbank::FiltaerParams; using tidbank::Ladder;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static const double FS = 48000.0;

struct Take { std::vector<float> L, R; };
// Run n samples; in(i, ch) is the input; params are set every `every` samples (and may change through `at`).
static Take run(FiltaerParams p, double secs, std::function<float(long, int)> in, int every = 512,
                std::function<void(long, FiltaerParams&)> at = nullptr) {
    Filtaer f; f.prepare(FS);
    const long n = (long) (secs * FS); Take t; t.L.resize((size_t) n); t.R.resize((size_t) n);
    for (long i = 0; i < n; ++i) {
        if (i % every == 0) { if (at) at(i, p); f.setParams(p); }
        const float x[2] = {in(i, 0), in(i, 1)}; float y[2];
        f.process(x, y); t.L[(size_t) i] = y[0]; t.R[(size_t) i] = y[1];
    }
    return t;
}
static float sine(double hz, double amp, long i) { return (float) (amp * std::sin(2.0 * M_PI * hz * (double) i / FS)); }
// Amplitude of one frequency over [a, b) seconds (single-bin DFT on an integer number of samples).
static double amp(const std::vector<float>& x, double hz, double a, double b) {
    const long i0 = (long) (a * FS), i1 = (long) (b * FS); double re = 0, im = 0;
    for (long i = i0; i < i1; ++i) { const double w = 2.0 * M_PI * hz * (double) i / FS; re += x[(size_t) i] * std::cos(w); im += x[(size_t) i] * std::sin(w); }
    return 2.0 * std::hypot(re, im) / (double) (i1 - i0);
}
static double db(double v) { return 20.0 * std::log10(v + 1e-30); }
static double rms(const std::vector<float>& x, double a, double b) {
    double s = 0; const long i0 = (long) (a * FS), i1 = (long) (b * FS);
    for (long i = i0; i < i1; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return std::sqrt(s / (double) (i1 - i0));
}
static double peak(const std::vector<float>& x) { double m = 0; for (float v : x) m = std::max(m, (double) std::fabs(v)); return m; }
// Frequency from upward zero crossings (interpolated) over [a, b) seconds.
static double freq(const std::vector<float>& x, double a, double b) {
    const long i0 = (long) (a * FS), i1 = (long) (b * FS); double first = -1, last = -1; int n = 0;
    for (long i = i0 + 1; i < i1; ++i)
        if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0) {
            const double t = (double) (i - 1) + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
            if (first < 0) first = t; else ++n;
            last = t;
        }
    return n > 0 ? (double) n * FS / (last - first) : 0.0;
}
// TPT one-pole magnitude^2 at f for cutoff fc (bilinear, prewarped at fc).
static double onePole2(double f, double fc) { const double r = std::tan(M_PI * f / FS) / std::tan(M_PI * fc / FS); return 1.0 / (1.0 + r * r); }

int main() {
    // 1. Linear response, resonance 0: LP and HP of one ladder match the bilinear 4-pole exactly (small signal, tanh ~ linear).
    for (double fc : {200.0, 1000.0, 8000.0})
        for (double f : {fc / 4, fc / 2, fc, 2 * fc, 4 * fc}) {
            if (f > 20000.0) continue;
            Ladder l; const double g = std::tan(M_PI * fc / FS); std::vector<float> lp, hp;
            for (long i = 0; i < (long) FS; ++i) { auto o = l.tick(sine(f, 1e-4, i), g, 0.0); lp.push_back((float) (o.lp * 1e4)); hp.push_back((float) (o.hp * 1e4)); }
            const double p = onePole2(f, fc), wantLp = db(p * p), wantHp = db((1 - p) * (1 - p));  // per pole |LP|^2 = p, |HP|^2 = 1 - p
            const double gotLp = db(amp(lp, f, 0.5, 1.0)), gotHp = db(amp(hp, f, 0.5, 1.0));
            CHECK(std::fabs(gotLp - wantLp) < 0.1, "LP fc %.0f f %.0f: %.2f dB, want %.2f", fc, f, gotLp, wantLp);
            CHECK(std::fabs(gotHp - wantHp) < 0.1 || wantHp < -80, "HP fc %.0f f %.0f: %.2f dB, want %.2f", fc, f, gotHp, wantHp);
            if (f == fc) std::printf("ladder fc %.0f: LP %.2f dB, HP %.2f dB at fc (want -12.04 both)\n", fc, gotLp, gotHp);
        }

    // 2. Self-oscillation starts at the knob's 0.8 (3 o'clock, MM p. 22) and starts from silence, as the Matriarch's does.
    for (double r : {0.78, 0.82, 1.0}) {
        FiltaerParams p; p.cutoffHz = 1000; p.res1 = p.res2 = (float) r;
        auto t = run(p, 4.0, [](long, int) { return 0.0f; });
        const double lvl = db(rms(t.L, 3.5, 4.0));
        std::printf("res %.2f, no input, cutoff 1 kHz: last 0.5 s %.1f dBFS\n", r, lvl);
        if (r < 0.8) CHECK(lvl < -150, "res %.2f should not self-oscillate: %.1f dBFS", r, lvl);
        else CHECK(lvl > -40, "res %.2f should self-oscillate from silence: %.1f dBFS", r, lvl);
    }

    // 3. At full resonance the tone sits at the cutoff and stays bounded (peak <= 1) from 20 Hz to the top clamp.
    for (double fc : {20.0, 100.0, 1000.0, 5000.0, 20000.0}) {
        FiltaerParams p; p.cutoffHz = (float) fc; p.res1 = p.res2 = 1.0f;
        auto t = run(p, 6.0, [](long i, int) { return i < 48 ? 0.1f : 0.0f; });
        const double f = freq(t.R, 4.0, 6.0), pk = peak(t.R), want = std::min(fc, Filtaer::kMaxFsRatio * FS);
        std::printf("res 1, cutoff %.0f Hz: tone %.1f Hz (%.2f %%), peak %.3f\n", fc, f, 100.0 * (f / want - 1.0), pk);
        CHECK(std::fabs(f / want - 1.0) < 0.03, "cutoff %.0f: self-oscillation at %.1f Hz", fc, f);
        CHECK(std::isfinite(pk) && pk <= 1.0, "cutoff %.0f: peak %.3f", fc, pk);
    }

    // 4. SPACING moves VCF 1 (left in STEREO) only, in octaves; clamped to +-5.8.
    for (double s : {-3.0, -1.0, 0.0, 1.0, 2.0, 4.0}) {
        FiltaerParams p; p.cutoffHz = 300; p.spacingOct = (float) s; p.res1 = p.res2 = 1.0f;
        auto t = run(p, 4.0, [](long i, int) { return i < 48 ? 0.1f : 0.0f; });
        const double fl = freq(t.L, 2.5, 4.0), fr = freq(t.R, 2.5, 4.0), got = std::log2(fl / fr);
        std::printf("spacing %+.1f oct: VCF 1 %.1f Hz, VCF 2 %.1f Hz, %+.3f oct\n", s, fl, fr, got);
        CHECK(std::fabs(got - s) < 0.03, "spacing %+.1f: %+.3f oct", s, got);
        CHECK(std::fabs(fr / 300.0 - 1.0) < 0.03, "spacing %+.1f moved VCF 2 to %.1f Hz", s, fr);
    }
    {
        Filtaer f; f.prepare(FS); FiltaerParams p; p.cutoffHz = 100; p.spacingOct = 10; f.setParams(p);
        const float x[2] = {0, 0}; float y[2]; f.process(x, y);
        CHECK(std::fabs(f.cutoff1Hz() / (100.0 * std::exp2(5.8)) - 1.0) < 1e-6, "spacing clamp: VCF 1 at %.1f Hz", f.cutoff1Hz());
    }
    {
        Filtaer f; f.prepare(FS); FiltaerParams p; p.cutoffHz = 2000; p.spacingOct = -10; f.setParams(p);
        const float x[2] = {0, 0}; float y[2]; f.process(x, y);
        CHECK(std::fabs(f.cutoff1Hz() / (2000.0 * std::exp2(-5.8)) - 1.0) < 1e-6, "spacing clamp -: VCF 1 at %.1f Hz", f.cutoff1Hz());
    }

    // 5. Bass loss against resonance, as measured on the Matriarch 2026-11-19 (bass-st-t1/t2/t3: 65 Hz, cutoff ~2.4-2.7 kHz, STEREO,
    //    per filter): about -11 dB at RES noon (10.7-10.9) and -14 to -15 dB at 3 o'clock (13.7-14.9), the same on both outputs. That is
    //    the plain ladder's 1/(1+k): k 2.5 -> -10.9 dB, k 4 -> -14.0 dB. Take 0017 (3.3 / 6.2 dB) is an outlier (open, unexplained), not the target.
    {
        auto lvl = [](float r) {
            FiltaerParams p; p.cutoffHz = 2433; p.res1 = p.res2 = r;
            auto t = run(p, 3.0, [](long i, int) { return sine(65.2, 0.1, i); });
            return std::make_pair(db(amp(t.L, 65.2, 2.0, 3.0)), db(amp(t.R, 65.2, 2.0, 3.0)));
        };
        const auto a = lvl(0.0f), noon = lvl(0.5f), three = lvl(0.8f);
        const double n[2] = {noon.first - a.first, noon.second - a.second}, o[2] = {three.first - a.first, three.second - a.second};
        std::printf("bass loss at 65.2 Hz, -20 dBFS, cutoff 2433 Hz, res 0 -> noon (0.5): L %.2f dB, R %.2f dB (Matriarch -10.7..-10.9)\n", n[0], n[1]);
        std::printf("bass loss at 65.2 Hz, -20 dBFS, cutoff 2433 Hz, res 0 -> 3 o'clock (0.8): L %.2f dB, R %.2f dB (Matriarch -13.7..-14.9)\n", o[0], o[1]);
        for (double d : n) CHECK(d < -10.5 && d > -11.2, "bass loss at noon %.2f dB, Matriarch -10.7..-10.9", d);
        for (double d : o) CHECK(d < -13.6 && d > -15.0, "bass loss at 3 o'clock %.2f dB, Matriarch -13.7..-14.9", d);
    }

    // 6. Routings. STEREO: left = VCF 1 (moves with SPACING), right = VCF 2. SERIES: HP then LP, a band-pass, L = R.
    //    PARALLEL: HP + LP, L = R, passes lows and highs. All three: a signal on the left input alone reaches both filters.
    {
        auto lvl = [](int mode, double spacing, double f, int ch, int inCh = -1) {
            FiltaerParams p; p.mode = mode; p.cutoffHz = 1000; p.spacingOct = (float) spacing;
            auto t = run(p, 1.0, [=](long i, int c) { return inCh < 0 || c == inCh ? sine(f, 0.01, i) : 0.0f; });
            return db(amp(ch == 0 ? t.L : t.R, f, 0.5, 1.0) / (inCh < 0 ? 0.01 : 0.005));
        };
        const double sL = lvl(FiltaerParams::Stereo, 2.0, 3000, 0), sR = lvl(FiltaerParams::Stereo, 2.0, 3000, 1);
        std::printf("stereo, spacing +2: 3 kHz L %.1f dB (VCF 1 at 4 kHz), R %.1f dB (VCF 2 at 1 kHz)\n", sL, sR);
        const double wL = db(onePole2(3000, 4000) * onePole2(3000, 4000)), wR = db(onePole2(3000, 1000) * onePole2(3000, 1000));
        CHECK(std::fabs(sL - wL) < 0.1 && std::fabs(sR - wR) < 0.1, "stereo routing: L %.2f (want %.2f), R %.2f (want %.2f)", sL, wL, sR, wR);
        const double b100 = lvl(FiltaerParams::Series, 0, 100, 0), b1k = lvl(FiltaerParams::Series, 0, 1000, 0), b10k = lvl(FiltaerParams::Series, 0, 10000, 1);
        std::printf("series, spacing 0: 100 Hz %.1f dB, 1 kHz %.1f dB, 10 kHz %.1f dB\n", b100, b1k, b10k);
        CHECK(b1k > -25.0 && b100 < b1k - 12.0 && b10k < b1k - 12.0, "series band-pass: %.1f / %.1f / %.1f", b100, b1k, b10k);
        const double p100 = lvl(FiltaerParams::Parallel, 0, 100, 0), p10k = lvl(FiltaerParams::Parallel, 0, 10000, 1);
        std::printf("parallel, spacing 0: 100 Hz %.2f dB, 10 kHz %.2f dB\n", p100, p10k);
        CHECK(std::fabs(p100) < 0.5 && std::fabs(p10k) < 0.5, "parallel passes lows and highs: %.2f / %.2f", p100, p10k);
        for (int mode : {0, 1, 2}) {  // a left-only input reaches the right output at half level (lvl() already scales by the 0.5)
            const double one = lvl(mode, 0, 700, 1, 0), both = lvl(mode, 0, 700, 1);
            CHECK(std::fabs(one - both) < 0.05, "mode %d: left-only input reaches the right output at %.2f dB, want %.2f", mode, one, both);
            FiltaerParams p; p.mode = mode; p.cutoffHz = 1000;
            auto t = run(p, 0.5, [](long i, int c) { return c == 0 ? sine(700, 0.01, i) : 0.0f; });
            if (mode != FiltaerParams::Stereo) { int diff = 0; for (size_t i = 0; i < t.L.size(); ++i) diff += t.L[i] != t.R[i]; CHECK(diff == 0, "mode %d: L != R in %d samples", mode, diff); }
        }
    }

    // 7. Mix 0: the dry signal passes bit-exact in stereo, whatever the filter does.
    {
        FiltaerParams p; p.mix = 0; p.res1 = p.res2 = 1; p.driveDb = 24; p.mode = FiltaerParams::Series;
        auto in = [](long i, int c) { uint32_t s = (uint32_t) (i * 2654435761u + 7u + (uint32_t) c * 99u); s ^= s >> 15; s *= 2246822519u; s ^= s >> 13; return (float) ((s & 0xFFFF) / 65536.0 - 0.5); };
        auto t = run(p, 1.0, in); int bad = 0;
        for (long i = 0; i < (long) t.L.size(); ++i) bad += t.L[(size_t) i] != in(i, 0) || t.R[(size_t) i] != in(i, 1);
        CHECK(bad == 0, "mix 0: %d samples differ from the dry input", bad);
    }

    // 8. A mode change does not click: for every pair of modes, with the two filters 3 octaves apart and lows and highs in the
    //    input, the biggest sample step around the switch stays within 1.1x the steady signal's before or after it.
    //    (Without the duck the steps are 2.5-100x: checked by removing it, 2026-11-18.)
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) {
            if (a == b) continue;
            FiltaerParams p; p.cutoffHz = 1500; p.spacingOct = -3; p.mode = a;
            auto t = run(p, 1.0, [](long i, int) { return sine(100, 0.3, i) + sine(3000, 0.1, i); }, 1,
                         [=](long i, FiltaerParams& q) { if (i >= 24011) q.mode = b; });
            auto step = [&](long i0, long i1) {
                double m = 0;
                for (long i = i0; i < i1; ++i) m = std::max({m, (double) std::fabs(t.L[(size_t) i] - t.L[(size_t) i - 1]), (double) std::fabs(t.R[(size_t) i] - t.R[(size_t) i - 1])});
                return m;
            };
            const double before = step(12000, 23000), sw = step(23900, 26000), after = step(36000, 47000);
            if (a == 1 && b == 0) std::printf("mode change stereo -> series: max step %.4f around the switch, %.4f / %.4f steady before / after\n", sw, before, after);
            CHECK(sw <= 1.1 * std::max(before, after), "mode change %d -> %d clicks: step %.4f vs %.4f / %.4f", a, b, sw, before, after);
        }

    // 9. Drive: small signals keep their level (within 0.5 dB at +24 dB drive); loud ones saturate (3rd harmonic up).
    {
        auto h = [](float drive, double a, double f) {
            FiltaerParams p; p.cutoffHz = 20000; p.driveDb = drive;
            auto t = run(p, 1.0, [=](long i, int) { return sine(100, a, i); });
            return db(amp(t.L, f, 0.5, 1.0) / a);
        };
        const double small0 = h(0, 0.001, 100), small24 = h(24, 0.001, 100), h3_0 = h(0, 0.3, 300), h3_24 = h(24, 0.3, 300);
        std::printf("drive: 100 Hz at -60 dBFS %.2f dB (0 dB) / %.2f dB (+24 dB); 3rd harmonic of -10 dBFS %.1f / %.1f dB\n", small0, small24, h3_0, h3_24);
        const double h3_full = h(0, 1.0, 300), h3_6 = h(0, 0.5, 300);
        std::printf("drive 0, cutoff 20 kHz: 3rd harmonic of a 100 Hz sine at 0 dBFS %.1f dB, at -6 dBFS %.1f dB, at -10 dBFS %.1f dB\n", h3_full, h3_6, h3_0);
        CHECK(std::fabs(small24 - small0) < 0.5, "drive changes small-signal level: %.2f vs %.2f", small24, small0);
        CHECK(h3_24 > -30.0 && h3_24 > h3_0 + 20.0, "drive does not saturate: %.1f vs %.1f", h3_24, h3_0);
    }

    // 10. Parameters set every 64 or 1000 samples give the same output (smoothing is per sample), and process() never allocates.
    {
        auto at = [](long i, FiltaerParams& q) { q.cutoffHz = i < 24000 ? 300.0f : 3000.0f; q.res1 = 0.6f; q.spacingOct = i < 8000 ? 0.0f : 1.5f; };
        auto in = [](long i, int) { return sine(220, 0.2, i); };
        FiltaerParams p; auto a = run(p, 1.0, in, 1000, at), b = run(p, 1.0, in, 64, at); int diff = 0;
        // 1000 and 64 both divide 24000 and 8000, so the targets change at the same samples
        for (size_t i = 0; i < a.L.size(); ++i) diff += a.L[i] != b.L[i] || a.R[i] != b.R[i];
        CHECK(diff == 0, "block size changes the output in %d samples", diff);
        Filtaer f; f.prepare(FS); f.setParams(p); allocs = 0; counting = true;
        for (long i = 0; i < 48000; ++i) { const float x[2] = {sine(220, 0.2, i), 0.0f}; float y[2]; if (i % 64 == 0) { p.cutoffHz = 200.0f + (float) (i % 5000); f.setParams(p); } f.process(x, y); }
        counting = false;
        CHECK(allocs == 0, "process allocated %ld times", allocs);
    }

    std::printf(fails ? "filtaer tests: %d FAILED\n" : "filtaer tests: all passed\n", fails);
    return fails ? 1 : 0;
}
