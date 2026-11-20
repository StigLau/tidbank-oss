// BBD profiles, filters and compander, and the feedback normalisation. The rest of the engine is judged by ears
// (make ears CASE=b).
#include "BbdEngine.h"
#include "BbdProfile.h"
#include "Compander.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <complex>
#include <vector>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
#define CHECKP(c, p) do { if (!(c)) { std::printf("FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, (p).id, #c); ++fails; } } while (0)
using namespace tidbank;

static double chainDb(const BbdProfile& p, int set, double f, double fs) {
    SosCascade a, r; a.design(p.aa[set], fs); r.design(p.recon[set], fs);
    return a.magDb(f, fs) + r.magDb(f, fs);
}
// First frequency above 200 Hz where the response (re 200 Hz) falls below `db` after its passband peak; 1 Hz grid.
static double crossHz(const BbdProfile& p, int set, double db, double fs, double scale = 1.0) {
    const double ref = chainDb(p, set, 200, fs);
    for (double f = 201; f < 0.45 * fs; f += 1) if (scale * (chainDb(p, set, f, fs) - ref) < db) return f;
    return -1;
}

// One pass of a steady sine through the engine at fb = 0 (compander on, noise and nonlinearity off): |out / in| at f.
static double passScale = 1.0;  // the feedback scale passGain's engine used (fbScale at that clock)
static double passGain(const BbdProfile& p, double fs, double tMs, double f, int droopComp = 0) {
    BbdEngine e; e.prepare(fs, p);
    BbdParams bp; bp.spaced = 1; bp.time_ms = (float) tMs; bp.feedback = 0.0f; bp.noise = 0; bp.nonlin = 0; bp.droopComp = droopComp;
    e.setParams(bp);
    const int n = (int) (fs * (tMs / 1000.0 + 0.8)), w0 = n - (int) (0.4 * fs);
    std::complex<double> X = 0.0, Y = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = 0.1 * std::sin(2.0 * M_PI * f * i / fs), y = e.process((float) x);
        if (i >= w0) { const auto ph = std::polar(1.0, -2.0 * M_PI * f * i / fs); X += x * ph; Y += y * ph; }
    }
    passScale = e.fbScale();
    return std::abs(Y) / std::abs(X);
}
// Short pulse into the engine (default params: compander, noise, nonlinearity on); peak |y| in [e0, e1) s and in the last second.
struct Peaks { double early, late; bool finite; };
static Peaks pulse(const BbdProfile& p, double fs, double tMs, double fb, double secs, double e0, double e1, int droopComp = 0) {
    BbdEngine e; e.prepare(fs, p);
    BbdParams bp; bp.time_ms = (float) tMs; bp.feedback = (float) fb; bp.droopComp = droopComp; e.setParams(bp);
    Peaks r{0.0, 0.0, true}; const int n = (int) (fs * secs);
    for (int i = 0; i < n; ++i) {
        const double x = i < 48 ? 0.5 * std::sin(M_PI * i / 48.0) : 0.0, y = e.process((float) x), t = i / fs;
        if (!std::isfinite(y)) { r.finite = false; break; }
        if (t >= e0 && t < e1) r.early = std::fmax(r.early, std::fabs(y));
        if (t >= secs - 1.0) r.late = std::fmax(r.late, std::fabs(y));
    }
    return r;
}
static double dB(double v) { return 20.0 * std::log10(v + 1e-30); }

// Plugin-equivalent mix (PluginProcessor::processBlock, BBD engine, output 0 dB): out = dry * in + echo * bbd(in), feedback =
// Intensity, delay = 1/8 at 120 BPM (plugin default sync) = 250 ms, Bright. Input: three 150 ms band-limited noise bursts
// (150-3500 Hz, -18 dBFS RMS, 5 ms fades) in the first second, then silence. Per-second RMS (dBFS) and overall peak.
static std::vector<float> burstInput(double fs, double secs) {
    std::vector<float> x((size_t) (fs * secs), 0.0f);
    std::vector<double> b(x.size(), 0.0);
    uint32_t rng = 12345u; double hp = 0, hpx = 0, lp1 = 0, lp2 = 0;
    const double aH = std::exp(-2 * M_PI * 150 / fs), aL = std::exp(-2 * M_PI * 3500 / fs), t0s[] = {0.10, 0.40, 0.70};
    auto env = [&](double t) {
        double v = 0;
        for (double t0 : t0s) if (t >= t0 && t < t0 + 0.15) v = std::fmin(1.0, std::fmin((t - t0) / 0.005, (t0 + 0.15 - t) / 0.005));
        return v;
    };
    double s = 0; size_t on = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        rng = rng * 1664525u + 1013904223u; const double w = (double) (int32_t) rng / 2147483648.0;
        hp = aH * (hp + w - hpx); hpx = w; lp1 = aL * lp1 + (1 - aL) * hp; lp2 = aL * lp2 + (1 - aL) * lp1;
        if (env(i / fs) > 0.0) { b[i] = lp2; s += lp2 * lp2; ++on; }
    }
    const double g = std::pow(10.0, -18.0 / 20.0) / std::sqrt(s / (double) on);
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (g * b[i] * env(i / fs));
    return x;
}
struct MixStats { double peakDb; std::vector<double> rmsDb; };
static MixStats pluginMix(const BbdProfile& p, double fs, const std::vector<float>& in, double intensity, double echo, double dry = 1.0) {
    BbdEngine e; e.prepare(fs, p);
    BbdParams bp; bp.time_ms = 250.0f; bp.feedback = (float) intensity; e.setParams(bp);
    MixStats m{-300.0, {}}; std::vector<double> ss(in.size() / (size_t) fs, 0.0); double pk = 0.0;
    for (size_t i = 0; i < in.size(); ++i) {
        const double y = dry * in[i] + echo * e.process(in[i]);
        pk = std::fmax(pk, std::fabs(y)); if (i / (size_t) fs < ss.size()) ss[i / (size_t) fs] += y * y;
    }
    m.peakDb = dB(pk); for (double v : ss) m.rmsDb.push_back(10.0 * std::log10(v / fs + 1e-30));
    return m;
}

int main() {
    const double fs = 48000.0;
    for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i);
        // Every number has a source line.
        for (const Fact* f : {&p.stages, &p.delayMinMs, &p.delayMaxMs, &p.spacedMaxMs, &p.chipClkMinHz, &p.chipClkMaxHz, &p.snrDb,
                              &p.compTauMs, &p.compRef, &p.nlA, &p.nlB, &p.fullScale, &p.wetBwHz[0], &p.wetBwHz[1]})
            CHECKP(f->ref && std::strlen(f->ref) > 0, p);
        CHECKP(p.filterRef && p.fbRef, p);
        // t_d = N / (2 f_clk): the clock at both ends of TIME, and whether that leaves the datasheet range.
        const double fHi = clockHz(p, p.delayMinMs.v), fLo = clockHz(p, p.delayMaxMs.v);
        CHECKP(p.overclock == (fHi > p.chipClkMaxHz.v + 1e-6), p);   // declared, never silently clamped
        CHECKP(p.underclock == (fLo < p.chipClkMinHz.v - 1e-6), p);
        CHECKP(p.delayMinMs.v < p.delayMaxMs.v && p.spacedMaxMs.v >= p.delayMaxMs.v, p);
        // Filters: stable, unity DC, low-pass.
        for (int s = 0; s < 2; ++s) {
            SosCascade a, r; a.design(p.aa[s], fs); r.design(p.recon[s], fs);
            CHECKP(a.sections() > 0 && r.sections() > 0 && a.stable() && r.stable(), p);
            CHECKP(std::fabs(a.magDb(1e-3, fs)) < 1e-6 && std::fabs(r.magDb(1e-3, fs)) < 1e-6, p);
            CHECKP(chainDb(p, s, 15000, fs) < -40.0, p);
            // Wet target (steady sine, compander doubles dB): the design meets it within 1 %.
            if (p.wetBwHz[s].v > 0) {
                const double f3 = crossHz(p, s, -3.0, fs, 2.0);
                CHECKP(std::fabs(f3 / p.wetBwHz[s].v - 1.0) < 0.01, p);
            }
        }
    }
    // Known numbers.
    {
        BbdProfile m; CHECK(findProfile("m8192", m));
        CHECK(std::fabs(clockHz(m, 780) - 5251.28) < 0.01);  // 8.1
        CHECK(std::fabs(clockHz(m, 35) - 117028.57) < 0.01);
        // d32/d33: measured TIME range and filter corners (bbd-research 8.1b).
        CHECK(m.delayMinMs.v == 41.65 && m.delayMaxMs.v == 404.18 && m.delayMinMs.s == Src::Measured);
        CHECK(m.wetBwHz[0].v == 2350 && m.wetBwHz[1].v == 1410 && m.wetBwHz[0].s == Src::Measured);
        BbdProfile d; CHECK(findProfile("d4096", d));
        CHECK(std::fabs(d.stages.v / (2.0 * 6800.0) * 1e3 - 301.18) < 0.01);  // DM-2 calibration clock (7)
        // RS10 second-order section 39 nF / 330 pF: 14.7 dB peak near 4.4 kHz (bbd-research 6 item 4), checked at 96 kHz.
        FilterDesign sk2; sk2.add2(39e-9, 330e-12); SosCascade c; c.design(sk2, 96000.0);
        double pk = -1e9, fpk = 0; for (double f = 1000; f < 10000; f += 5) { const double v = c.magDb(f, 96000.0); if (v > pk) { pk = v; fpk = f; } }
        CHECK(std::fabs(pk - 14.7) < 0.3 && std::fabs(fpk - 4437) < 120);
        // Corrected 3rd-order a1 (BbdFilter.h) reproduces RS10 Fig. 5: anti-alias ~4.5 dB, 3rd-order reconstruction ~6 dB.
        auto peakDb = [](const FilterDesign& fd) { SosCascade s; s.design(fd, 96000.0); double ref = s.magDb(200, 96000.0), m = -1e9;
            for (double f = 201; f < 20000; f += 5) m = std::fmax(m, s.magDb(f, 96000.0) - ref); return m; };
        FilterDesign aa3; aa3.add3(6.8e-9, 82e-9, 330e-12);
        FilterDesign re3; re3.add3(2.2e-9, 33e-9, 1e-9);
        CHECK(std::fabs(peakDb(aa3) - 4.5) < 0.5 && std::fabs(peakDb(re3) - 6.0) < 0.5);
        // DM-2 chain -3 dB: 3.2-3.3 kHz (bbd-research 3.1, +-10 %). OPEN: the chain peaks +11.6 dB near 2 kHz (finding).
        const double f3 = crossHz(d, 0, -3.0, 96000.0);
        CHECK(f3 > 0.9 * 3200 && f3 < 1.1 * 3300);
        BbdProfile x; CHECK(!findProfile("nope", x));
    }
    // Compander: steady sine through compress -> expand returns at its own level (8.6), across 40 dB.
    for (double A : {0.01, 0.1, 0.5}) {
        Compander c; c.prepare(fs, 2.2, 0.25);
        double sIn = 0, sOut = 0; const int N = (int) (2 * fs);
        for (int n = 0; n < N; ++n) {
            const double x = A * std::sin(2.0 * M_PI * 200.0 * n / fs), y = c.expand(c.compress(x));
            if (n > N / 2) { sIn += x * x; sOut += y * y; }
        }
        CHECK(std::fabs(10.0 * std::log10(sOut / sIn)) < 0.1);
    }
    // Below the knee (compressor at its +40 dB cap) the round trip is still unity: the expander has the same floor.
    for (double A : {1e-6, 1e-5, 2e-5}) {  // steady-state knee near A = 3.9e-5 (200 Hz, ref 0.25, cap 40 dB)
        Compander c; c.prepare(fs, 2.2, 0.25);
        double sIn = 0, sOut = 0; const int N = (int) (2 * fs);
        for (int n = 0; n < N; ++n) {
            const double x = A * std::sin(2.0 * M_PI * 200.0 * n / fs), y = c.expand(c.compress(x));
            if (n > N / 2) { sIn += x * x; sOut += y * y; }
        }
        CHECK(std::fabs(10.0 * std::log10(sOut / sIn)) < 0.1);
    }
    // Compressor is 2:1: 20 dB in -> 10 dB out (steady state).
    {
        auto lvl = [&](double A) { Compander c; c.prepare(fs, 2.2, 0.25); double s = 0; const int N = (int) fs;
            for (int n = 0; n < N; ++n) { const double y = c.compress(A * std::sin(2.0 * M_PI * 200.0 * n / fs)); if (n > N / 2) s += y * y; }
            return 10.0 * std::log10(s); };
        CHECK(std::fabs((lvl(0.1) - lvl(0.01)) - 10.0) < 0.2);
    }
    // Expander rectifier: step of |x| reaches 63 % in tau (one-pole, 8.6).
    {
        Compander c; c.prepare(fs, 2.2, 0.25);
        for (int n = 0; n < (int) fs; ++n) c.expand(0.0);
        const double start = c.expAvg(); int n63 = -1;
        for (int n = 0; n < (int) fs && n63 < 0; ++n) { c.expand(1.0); if (c.expAvg() - start >= 0.632 * (1.0 - start)) n63 = n + 1; }
        CHECK(std::fabs(n63 / fs * 1e3 - 2.2) < 0.03);
    }
    // feedback normalised per profile, so Intensity = peak loop gain in every profile ("loop gain <= intensity").
    for (double sr : {44100.0, 48000.0, 96000.0}) for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i);
        // Independent recompute on a 1 Hz grid of the designed chain, both sets (an upper bound for the Bright/Dark blend
        // only matters for m8192, whose sets both stay <= 0 dB).
        double pk = -1e9;
        for (int s = 0; s < (p.hasDark ? 2 : 1); ++s) for (double f = 1; f < 0.5 * sr; f += 1) pk = std::fmax(pk, chainDb(p, s, f, sr));
        const double g = std::pow(10.0, pk / 20.0);
        BbdEngine e; e.prepare(sr, p);
        if (!std::strcmp(p.id, "m8192")) {
            CHECKP(e.loopScale(true) == 1.0 && e.loopScale(false) == 1.0, p);  // exactly unchanged
        } else {
            CHECKP(std::fabs(e.loopScale(true) * g * g - 1.0) < 1e-4, p);
            CHECKP(std::fabs(e.loopScale(false) * g - 1.0) < 1e-4, p);
            CHECKP(g > 3.8 && g < 3.95, p);  // +11.8 dB chain peak near 1.96 kHz
        }
        if (sr == 48000.0) std::printf("loop norm %s: peak |H| %.4f (%+.2f dB) -> scale %.6f (compander on), %.6f (bypassed)\n",
                                       p.id, g, 20 * std::log10(g), e.loopScale(true), e.loopScale(false));
    }
    // Measured through the engine: one-pass steady-sine gain x scale <= 1 at both ends of TIME (the invariant itself).
    for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i); BbdEngine e; e.prepare(fs, p);
        for (double t : {p.delayMinMs.v, p.delayMaxMs.v, p.spacedMaxMs.v}) {
            double worst = 0.0, fw = 0.0;
            for (double f = 100; f <= 5000; f += 50) { const double v = passGain(p, fs, t, f) * e.loopScale(true); if (v > worst) { worst = v; fw = f; } }
            std::printf("loop gain %s T=%.0f ms: max %.4f at %.0f Hz (x Intensity)\n", p.id, t, worst, fw);
            CHECKP(worst <= 1.0 + 1e-3, p);
        }
    }
    // d36: the output hold moves the runaway point with the delay (authentic, default); droopComp = 1 undoes it per clock.
    // Peak loop gain (x Intensity) at TIME min, the geometric mid and the longest delay: authentic d4096 / c8192 fall to ~0.76 at a 6.8 kHz clock;
    // compensated stays within 0.98..1.01 on every profile (1.004 at the lowest clock: the hold's images reach the rectifiers).
    for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i); const bool m = !std::strcmp(p.id, "m8192");
        for (double t : {p.delayMinMs.v, std::sqrt(p.delayMinMs.v * p.spacedMaxMs.v), p.spacedMaxMs.v}) {  // ends + one table interior point
            auto peak = [&](int dc, double f0, double f1) {  // 200 Hz grid, then 20 Hz around the best point
                double best = 0.0, fb = f0;
                for (double f = f0; f <= f1; f += 200) { const double v = passGain(p, fs, t, f, dc) * passScale; if (v > best) { best = v; fb = f; } }
                for (double f = fb - 180; f <= fb + 180; f += 20) best = std::fmax(best, passGain(p, fs, t, f, dc) * passScale);
                return best; };
            const double comp = peak(1, 100, 4000), auth = m ? 0.0 : peak(0, 1500, 2300);  // d4096 / c8192 peak near 1.92-1.96 kHz
            std::printf("droop %s T=%.0f ms: peak loop gain compensated %.3f, authentic %.3f (x Intensity; not swept on m8192)\n", p.id, t, comp, auth);
            CHECKP(comp > 0.98 && comp < 1.01, p);
            if (t > 250.0 && !m) CHECKP(auth < 0.8, p);
        }
    }
    // Runaway at the longest delay, d4096 (300 ms), pulse at -6 dBFS: authentic decays at 1.2 (last second < -60 dBFS) and runs
    // away at 1.4, the raised Intensity max (> -20 dBFS after 30 s); compensated runs away at 1.1 (> -20 dBFS after 20 s).
    {
        BbdProfile p; CHECK(findProfile("d4096", p));
        const Peaks a12 = pulse(p, fs, 300.0, 1.2, 20.0, 0.3, 1.2), a14 = pulse(p, fs, 300.0, 1.4, 30.0, 0.3, 1.2);
        const Peaks c11 = pulse(p, fs, 300.0, 1.1, 20.0, 0.3, 1.2, 1);
        std::printf("droop d4096 T=300 ms, last second: authentic I=1.2 %.1f dBFS, I=1.4 %.1f dBFS; compensated I=1.1 %.1f dBFS\n",
                    dB(a12.late), dB(a14.late), dB(c11.late));
        CHECK(a12.finite && a14.finite && c11.finite);
        CHECK(dB(a12.late) < -60.0);
        CHECK(dB(a14.late) > -20.0);
        CHECK(dB(c11.late) > -20.0);
    }
    // Impulse at Intensity 0.95 decays (early window vs the last second of a 60-repeat + 2 s run): >= 40 dB for d4096 / c8192,
    // >= 20 dB for m8192, whose flat filters lose almost nothing per pass (measured 26 / 22 / 26 dB at 42 / 200 / 404 ms).
    // Before the expander fix, m8192 managed only 2 / 6 / 13 dB, settling into a tone near the compressor's cap at 42 ms (no expander
    // floor). At 1.2 it still runs away (200 ms) and stays finite, bounded.
    for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i); const bool m = !std::strcmp(p.id, "m8192");
        for (double t : {p.delayMinMs.v, 200.0, p.delayMaxMs.v}) {
            const double T = t / 1000.0; const Peaks r = pulse(p, fs, t, 0.95, 60 * T + 2.0, T, 4 * T);
            std::printf("impulse %s T=%.0f ms I=0.95: early %.1f dB, late %.1f dB\n", p.id, t, dB(r.early), dB(r.late));
            CHECKP(r.finite, p);
            CHECKP(dB(r.late) < dB(r.early) - (m ? 20.0 : 40.0), p);
        }
        const Peaks r = pulse(p, fs, 200.0, 1.2, 20.0, 0.2, 0.8);
        std::printf("impulse %s T=200 ms I=1.2: early %.1f dB, late %.1f dB\n", p.id, dB(r.early), dB(r.late));
        CHECKP(r.finite && dB(r.late) > dB(r.early) && dB(r.late) < 30.0, p);
    }
    // Idle (no input, noise on, 100 ms, peak over seconds 5-10; the first seconds carry the rectifiers' start-up state): at
    // Intensity 0.95 the BBD noise stays below -100 dBFS on every profile (-109 / -107 / -104 dBFS); m8192 at 1.2 builds
    // self-oscillation from noise alone (no filter loss; since the expander fix the expander passes the noise instead of gating it).
    for (int i = 0; i < profileCount(); ++i) {
        const BbdProfile p = profileAt(i);
        auto idlePeak = [&](double fb) { BbdEngine e; e.prepare(fs, p); BbdParams bp; bp.time_ms = 100.0f; bp.feedback = (float) fb;
            e.setParams(bp); double pk = 0; for (int n = 0; n < (int) (10 * fs); ++n) { const double y = e.process(0.0f); if (n >= (int) (5 * fs)) pk = std::fmax(pk, std::fabs(y)); }
            return dB(pk); };
        const double q = idlePeak(0.95); std::printf("idle %s I=0.95: peak %.1f dBFS\n", p.id, q); CHECKP(q < -100.0, p);
        if (!std::strcmp(p.id, "m8192")) { const double r = idlePeak(1.2); std::printf("idle %s I=1.2: peak %.1f dBFS\n", p.id, r); CHECKP(r > -60.0, p); }
    }
    // Plugin-equivalent mix, 6 s: defaults (Intensity 0.45, Echo 0.5, Dry 1) and the maintainer's Live settings (2026-11-16: D-4096
    // I 0.6 / E 0.14, C-8192 I 0.05 / E 0.14) on every profile. Must stay <= +6 dBFS and decay (last second at least 30 dB
    // under the loudest second and under -60 dBFS). Before per-profile normalisation, d4096 and c8192 sustained near +16 dBFS RMS at the defaults.
    {
        const std::vector<float> in = burstInput(fs, 6.0);
        struct Cfg { const char* name; double I, E; } cfgs[] = {{"default I0.45 E0.50", 0.45, 0.5}, {"I0.60 E0.14", 0.6, 0.14}, {"I0.05 E0.14", 0.05, 0.14}};
        for (int i = 0; i < profileCount(); ++i) for (const auto& c : cfgs) {
            const BbdProfile p = profileAt(i); const MixStats ms = pluginMix(p, fs, in, c.I, c.E);
            double mx = -1e9; for (double v : ms.rmsDb) mx = std::fmax(mx, v);
            std::printf("mix %s %-19s peak %+6.1f dBFS, RMS/s:", p.id, c.name, ms.peakDb);
            for (double v : ms.rmsDb) std::printf(" %6.1f", v);
            std::printf("\n");
            CHECKP(ms.peakDb <= 6.0, p);
            CHECKP(ms.rmsDb.back() < mx - 30.0 && ms.rmsDb.back() < -60.0, p);
        }
    }
    std::printf(fails ? "bbd: %d FAILED\n" : "bbd: all passed\n", fails);
    return fails ? 1 : 0;
}
