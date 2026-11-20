// Reels v0 core (dsp/Reel.h): timing, bit-exact playback, seams, layers, transport, allocation. JUCE-free, seconds.
// Cases follow docs/plan-looper.md section 5 (l0_*, l1_*, l3_mixed_lengths), measured in C++ instead of through the ears renderer.
#include "Reel.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <new>
#include <vector>

static long allocs = 0;
static bool counting = false;
void* operator new(std::size_t n) { if (counting) ++allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { if (counting) ++allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using tidbank::Reel; using tidbank::ReelHost; using tidbank::ReelParams;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// Host simulation: ppq from the host sample position, parameters per block.
struct Sim {
    Reel r; double sr; int N; double bpm; bool hasPpq = true, playing = true;
    int64_t pos = 0;                         // host sample position (advances only while playing)
    ReelParams p;
    std::function<float(int64_t)> in = [](int64_t) { return 0.0f; };
    std::vector<float> out;                  // left channel, one entry per processed sample
    double tailRms(size_t n) const { double a = 0; for (size_t i = out.size() - n; i < out.size(); ++i) a += (double) out[i] * out[i]; return std::sqrt(a / (double) n); }
    std::vector<int64_t> at;                 // host position of each output sample
    Sim(double sr_, double bpm_, int N_, double pool = 60.0) : sr(sr_), N(N_), bpm(bpm_) { r.prepare(sr, pool, 2); }
    void run(int64_t n) {
        for (int64_t done = 0; done < n;) {
            const int len = (int) std::min<int64_t>(N, n - done);
            ReelHost h; h.bpm = bpm; h.playing = playing; h.hasPpq = hasPpq; h.ppq = (double) pos * bpm / (60.0 * sr);
            r.setBlock(h, p);
            for (int i = 0; i < len; ++i) {
                const float x = in(pos); const float xi[2] = {x, x}; float y[2];
                r.process(xi, y);
                out.push_back(y[0]); at.push_back(pos);
                if (playing) ++pos;
            }
            done += len;
        }
    }
    void trigger(bool ReelParams::*f) { p.*f = true; run(N); p.*f = false; run(N); }
    double rms(int64_t from, int64_t to) const {  // over host positions [from, to)
        double s = 0; int64_t n = 0;
        for (size_t i = 0; i < out.size(); ++i) if (at[i] >= from && at[i] < to) { s += (double) out[i] * out[i]; ++n; }
        return n ? std::sqrt(s / (double) n) : 0.0;
    }
    float outAt(int64_t host) const { for (size_t i = out.size(); i-- > 0;) if (at[i] == host) return out[i]; return NAN; }
};
static double db(double x) { return 20.0 * std::log10(std::max(x, 1e-30)); }
static float noise(int64_t n) { uint32_t s = (uint32_t) (n * 2654435761u + 12345u); s ^= s >> 13; s *= 1274126177u; s ^= s >> 16; return (float) ((s & 0xFFFF) / 65536.0 - 0.5) * 0.5f; }

// l0_idle: nothing recorded = silence
static void idle() {
    Sim s(48000, 120, 64); s.in = noise; s.run(48000);
    double peak = 0; for (float v : s.out) peak = std::max(peak, (double) std::fabs(v));
    CHECK(peak == 0.0, "idle: reel output %g, want exactly 0", peak);
}

// l0_null: 120 BPM at 48 kHz (1 bar = 96000 samples exactly): pass 2 replays pass 0 bit-exact outside the punch fades
static void null() {
    Sim s(48000, 120, 64); s.in = [](int64_t n) { return n < 96000 ? noise(n) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(96000); s.p.rec = false; s.run(3 * 96000);
    int bad = 0; for (int64_t m = 300; m < 96000 - 300; ++m) if (s.outAt(2 * 96000 + m) != noise(m)) ++bad;
    CHECK(bad == 0, "null: %d cells of pass 2 differ from the recorded input (want bit-exact)", bad);
    CHECK(s.r.probe(Reel::kLayers) == 1 && s.r.probe(Reel::kRecording) == 0, "null: layers %g recording %g", s.r.probe(Reel::kLayers), s.r.probe(Reel::kRecording));
}

// l1_nodrift: an impulse recorded at sample 5000 of pass 0 lands at round(k L) + 5000 on every pass, 64 passes; no accumulation
static void nodrift(double sr, double bpm, int N) {
    Sim s(sr, bpm, N, 200.0); const double L = 4 * 60.0 / bpm * sr;
    s.in = [](int64_t n) { return n == 5000 ? 1.0f : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run((int64_t) L + N); s.p.rec = false;
    s.run((int64_t) (65 * L));
    int worst = 0, maxErrPass = -1; double worstTrue = 0;
    for (int k = 1; k < 64; ++k) {
        const int64_t a = (int64_t) std::llround(k * L), b = (int64_t) std::llround((k + 1) * L);
        int64_t arg = -1; float best = 0;
        for (size_t i = 0; i < s.out.size(); ++i) if (s.at[i] >= a && s.at[i] < b && std::fabs(s.out[i]) > best) { best = std::fabs(s.out[i]); arg = s.at[i]; }
        const int err = (int) std::llabs(arg - (a + 5000));
        if (err > worst) { worst = err; maxErrPass = k; }
        worstTrue = std::max(worstTrue, std::fabs((double) arg - (k * L + 5000.0)));
    }
    CHECK(worst == 0, "nodrift %g BPM %g Hz: impulse off round(kL)+5000 by %d samples (pass %d)", bpm, sr, worst, maxErrPass);
    CHECK(worstTrue <= 1.0, "nodrift %g BPM %g Hz: impulse %.3f samples off k L + 5000 (want <= 1)", bpm, sr, worstTrue);
    std::printf("nodrift %g BPM %g Hz: L = %.4f, 63 passes on round(kL)+5000, worst |t - (kL+5000)| = %.3f smp\n", bpm, sr, L, worstTrue);
}

// l1_seam_click: a tone with a whole number of cycles per loop stays a clean sine across every seam (97 BPM, 44.1 kHz)
static void seam() {
    const double sr = 44100, bpm = 97, L = 4 * 60.0 / bpm * sr, f = 1237.0 * sr / L, w = 2 * M_PI * f / sr;
    Sim s(sr, bpm, 64, 200.0);
    s.in = [&](int64_t n) { return n < (int64_t) std::llround(L) + 2000 ? (float) (0.1 * std::sin(w * (double) n)) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run((int64_t) std::llround(L)); s.p.rec = false; s.run((int64_t) (18 * L));
    // residual of the sine recurrence y[n] - 2 cos(w) y[n-1] + y[n-2] = 0, near each seam and mid-pass
    auto resid = [&](int64_t from, int64_t to) {
        double m = 0;
        for (size_t i = 2; i < s.out.size(); ++i) if (s.at[i] >= from && s.at[i] < to)
            m = std::max(m, std::fabs((double) s.out[i] - 2 * std::cos(w) * s.out[i - 1] + s.out[i - 2]));
        return m;
    };
    double seamWorst = 0, midWorst = 0;
    for (int k = 2; k < 18; ++k) {
        const int64_t a = (int64_t) std::llround(k * L);
        seamWorst = std::max(seamWorst, resid(a - 400, a + 400));
        midWorst = std::max(midWorst, resid(a + 40000, a + 40800));
    }
    CHECK(db(seamWorst) < -60.0, "seam: sine residual at seams %.1f dBFS (want < -60; mid-pass %.1f)", db(seamWorst), db(midWorst));
    std::printf("seam 97 BPM 44.1 kHz: residual at seams %.1f dBFS, mid-pass %.1f dBFS\n", db(seamWorst), db(midWorst));
}

// l0_overdub / l0_undo_redo: two layers of the same tone = +6.02 dB; undo, redo; a new take after undo replaces the undone layer
static void layers() {
    const double sr = 48000; const int64_t L = 96000; const double w = 2 * M_PI * 500.0 / sr;
    Sim s(sr, 120, 64);
    bool feed = true; s.in = [&](int64_t n) { return feed ? (float) (0.1 * std::sin(w * (double) n)) : 0.0f; };
    s.p.bars = 1;
    s.p.rec = true; s.run(L); s.p.rec = false; feed = false; s.run(L);           // layer 1 in pass 0
    const double one = s.rms(L + 1000, 2 * L - 1000);
    feed = true; s.p.rec = true; s.run(L); s.p.rec = false; feed = false; s.run(L);  // layer 2 in pass 2
    const double two = s.rms(3 * L + 1000, 4 * L - 1000);
    CHECK(std::fabs(db(two / one) - 6.0206) < 0.05, "overdub: %.3f dB, want +6.02", db(two / one));
    CHECK(s.r.probe(Reel::kLayers) == 2, "overdub: %g layers, want 2", s.r.probe(Reel::kLayers));
    s.trigger(&ReelParams::undo); s.run(L);
    const double und = s.rms(s.pos - L + 1000, s.pos - 1000);
    CHECK(std::fabs(db(und / one)) < 0.05, "undo: %.3f dB vs one layer", db(und / one));
    s.trigger(&ReelParams::redo); s.run(L);
    const double red = s.rms(s.pos - L + 1000, s.pos - 1000);
    CHECK(std::fabs(db(red / one) - 6.0206) < 0.05, "redo: %.3f dB vs one layer, want +6.02", db(red / one));
    s.trigger(&ReelParams::undo);
    feed = true; s.p.rec = true; s.run(L); s.p.rec = false; feed = false; s.run(L);
    CHECK(s.r.probe(Reel::kLayers) == 2 && s.r.probe(Reel::kHeard) == 2, "new take after undo: layers %g heard %g, want 2 2",
          s.r.probe(Reel::kLayers), s.r.probe(Reel::kHeard));
}

// l3_mixed_lengths: a 1-bar click layer plays 4 times under a 4-bar layer
static void mixed() {
    const double sr = 48000; const int64_t L1 = 96000;
    Sim s(sr, 120, 64, 60.0);
    bool feed = true; s.in = [&](int64_t n) { return feed && n == 1000 ? 1.0f : feed && n == 4 * L1 + 50000 ? 0.5f : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(L1); s.p.rec = false; s.run(3 * L1);
    s.p.bars = 4; s.p.rec = true; s.run(4 * L1); s.p.rec = false; feed = false; s.run(4 * L1);
    int clicks = 0, half = 0;
    for (size_t i = 0; i < s.out.size(); ++i) if (s.at[i] >= 8 * L1 && s.at[i] < 12 * L1) {
        if (s.out[i] == 1.0f) { ++clicks; CHECK((s.at[i] - 1000) % L1 == 0, "mixed: click at %lld off the 1-bar grid", (long long) s.at[i]); }
        if (s.out[i] == 0.5f) { ++half; CHECK(s.at[i] == 8 * L1 + 50000, "mixed: 4-bar impulse at %lld", (long long) s.at[i]); }
    }
    CHECK(clicks == 4 && half == 1, "mixed: %d clicks and %d 4-bar impulses in 4 bars, want 4 and 1", clicks, half);
}

// l1_stop_start / l1_locate: stop fades out and halts; start and locate resume on the grid of the host position
static void transport() {
    const double sr = 48000; const int64_t L = 96000;
    Sim s(sr, 120, 64);
    s.in = [](int64_t n) { return n == 5000 ? 1.0f : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(L); s.p.rec = false; s.run(L / 2);
    s.playing = false; s.run(4800);
    double tail = 0; for (size_t i = s.out.size() - 3840; i < s.out.size(); ++i) tail = std::max(tail, (double) std::fabs(s.out[i]));
    CHECK(tail == 0.0, "stop: reel 20..100 ms after stop %g, want 0", tail);
    CHECK(s.r.probe(Reel::kRunning) == 0, "stop: still running");
    s.playing = true; s.run(L);
    const size_t n0 = s.out.size(); s.run(L);
    int64_t hit = -1; for (size_t i = n0; i < s.out.size(); ++i) if (s.out[i] > 0.5f) hit = s.at[i];
    CHECK(hit >= 0 && (hit - 5000) % L == 0, "start: impulse at host %lld, want 5000 + k L", (long long) hit);
    // locate: jump the host to 2 bars + 40000
    s.pos = 10 * L + 40000; s.run(2 * L);
    CHECK(s.r.probe(Reel::kSplices) == 1, "locate: splices %g, want 1", s.r.probe(Reel::kSplices));
    int64_t last = -1; for (size_t i = s.out.size() - (size_t) L; i < s.out.size(); ++i) if (s.out[i] > 0.5f) last = s.at[i];
    CHECK(last >= 0 && (last - 5000) % L == 0, "locate: impulse at host %lld, want 5000 + k L", (long long) last);
    // host loop of 4 bars around a 1-bar layer: the phase is unchanged, so no splice
    s.pos = s.pos - 4 * L; s.run(L);
    CHECK(s.r.probe(Reel::kSplices) == 1, "host loop: splices %g, want still 1 (phase kept, no fade)", s.r.probe(Reel::kSplices));
}

// l1_block_invariant: the same output at block sizes 1, 64 and 1000 (97 BPM, 44.1 kHz, noise layer)
static void blocks() {
    std::vector<float> ref;
    for (int N : {1, 64, 1000}) {
        Sim s(44100, 97, N, 200.0); s.in = [](int64_t n) { return n < 112000 ? noise(n) : 0.0f; };
        s.p.bars = 1; s.p.rec = true; s.run(112000); s.p.rec = false; s.run(400000);
        if (ref.empty()) ref = s.out;
        else { int d = 0; for (size_t i = 0; i < ref.size(); ++i) d += ref[i] != s.out[i]; CHECK(d == 0, "blocks: N=%d differs from N=1 in %d samples", N, d); }
    }
}

// l0_full: a 3 s pool takes one 2 s layer and refuses the second (flag), never truncates
static void full() {
    Sim s(48000, 120, 64, 3.0); s.in = noise; s.p.bars = 1;
    s.p.rec = true; s.run(96000); s.p.rec = false; s.run(6400);
    s.p.rec = true; s.run(9600); s.p.rec = false; s.run(640);
    CHECK(s.r.probe(Reel::kLayers) == 1 && s.r.probe(Reel::kFull) == 1, "full: layers %g full %g, want 1 1", s.r.probe(Reel::kLayers), s.r.probe(Reel::kFull));
}

// v0 has no varispeed: a tempo change mutes the layer (flag), the tempo back makes it audible again
static void tempo() {
    Sim s(48000, 120, 64); s.in = [](int64_t n) { return n < 96000 ? noise(n) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(96000); s.p.rec = false; s.run(96000);
    s.bpm = 126; s.pos = (int64_t) std::llround((double) s.pos * 120.0 / 126.0);  // same ppq at the new tempo
    s.run(4800);
    const double muted = s.tailRms(2400);
    CHECK(muted == 0.0 && s.r.probe(Reel::kTempoMuted) == 1, "tempo: rms %g muted flag %g, want 0 and 1", muted, s.r.probe(Reel::kTempoMuted));
    s.bpm = 120; s.pos = (int64_t) std::llround((double) s.pos * 126.0 / 120.0); s.run(96000);
    CHECK(s.tailRms(48000) > 0.05 && s.r.probe(Reel::kTempoMuted) == 0, "tempo back: rms %g flag %g", s.tailRms(48000), s.r.probe(Reel::kTempoMuted));
}

// Nudge. Frequency from interpolated positive zero crossings, median period over stretches louder than `floor`.
static double freq(const std::vector<float>& y, size_t from, size_t to, double sr, double floor = 0.02) {
    std::vector<double> t, per;
    for (size_t i = from + 1; i < to; ++i) if (y[i - 1] < 0.0f && y[i] >= 0.0f) {
        double pk = 0; for (size_t j = i; j < std::min(to, i + 40); ++j) pk = std::max(pk, (double) std::fabs(y[j]));
        if (pk > floor) t.push_back((double) (i - 1) + y[i - 1] / (double) (y[i - 1] - y[i]));
    }
    for (size_t k = 1; k < t.size(); ++k) if (t[k] - t[k - 1] < 200) per.push_back(t[k] - t[k - 1]);
    if (per.empty()) return 0;
    std::nth_element(per.begin(), per.begin() + per.size() / 2, per.end());
    return sr / per[per.size() / 2];
}

// t82_motor: the hand reaches 2x in 60 ms, the motor is back within 1 % in 400 ms; at rest the reel plays raw again, offset kept
// (grid option (b)) through stop/start, and Clear puts it back on the grid.
static void nudgeMotor() {
    const int64_t L = 96000;
    Sim s(48000, 120, 64); s.in = [&](int64_t n) { return n < L ? noise(n) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(L); s.p.rec = false; s.run(9600);
    s.p.nudge = 1.0f; s.run(2880);
    CHECK(std::fabs(s.r.probe(Reel::kSpeed) - 2.0) < 0.011, "nudge: speed %.4f after 60 ms of hand, want 2 within 1 %%", s.r.probe(Reel::kSpeed));
    s.run(9600);
    s.p.nudge = 0.0f; s.run(19200);
    CHECK(std::fabs(s.r.probe(Reel::kSpeed) - 1.0) < 0.0101, "nudge: speed %.4f 400 ms after release, want 1 within 1 %%", s.r.probe(Reel::kSpeed));
    s.run(L);
    const double off = s.r.probe(Reel::kOffset);
    CHECK(s.r.probe(Reel::kSpeed) == 1.0 && off == std::floor(off) && off > 9600, "nudge: at rest speed %.9f offset %.6f, want 1 and a whole number > 9600",
          s.r.probe(Reel::kSpeed), off);
    auto rawBad = [&](size_t n) {  // the last n outputs replay the recorded cells raw, shifted by the offset
        int bad = 0;
        for (size_t i = s.out.size() - n; i < s.out.size(); ++i) {
            const int64_t m = (((s.at[i] + (int64_t) off) % L) + L) % L;
            if (m >= 300 && m < L - 300 && s.out[i] != noise(m)) ++bad;
        }
        return bad;
    };
    s.run(L);
    CHECK(rawBad(L) == 0, "nudge: %d samples differ from raw playback at offset %.0f", rawBad(L), off);
    s.playing = false; s.run(4800); s.playing = true; s.run(2 * L);
    CHECK(s.r.probe(Reel::kOffset) == off && rawBad(L) == 0, "nudge: offset %.3f after stop/start (want %.0f kept), %d samples off raw",
          s.r.probe(Reel::kOffset), off, rawBad(L));
    s.trigger(&ReelParams::clear); s.run(9600);
    CHECK(s.r.probe(Reel::kOffset) == 0.0, "nudge: offset %.3f after Clear, want 0", s.r.probe(Reel::kOffset));
}

// t82_brake: hand at -1 stops the reel; the playback EQ takes the frozen value to silence; release brings the layer back
static void nudgeBrake() {
    const int64_t L = 96000;
    Sim s(48000, 120, 64); s.in = [&](int64_t n) { return n < L ? noise(n) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(L); s.p.rec = false; s.run(9600);
    const double before = s.tailRms(4800);
    s.p.nudge = -1.0f; s.run(48000);
    CHECK(s.r.probe(Reel::kSpeed) < 1e-3, "brake: speed %.5f after 1 s, want 0", s.r.probe(Reel::kSpeed));
    CHECK(db(s.tailRms(12000) / before) < -60.0, "brake: %.1f dB re playing, 0.75-1 s into the brake (want < -60)", db(s.tailRms(12000) / before));
    CHECK(s.r.probe(Reel::kOffset) < -40000, "brake: offset %.0f, want < -40000", s.r.probe(Reel::kOffset));
    s.p.nudge = 0.0f; s.run(96000);
    CHECK(std::fabs(db(s.tailRms(48000) / before)) < 1.0, "brake release: %.2f dB re before, want within 1 dB", db(s.tailRms(48000) / before));
}

// t82_pitch: hand at +0.5 plays a 1 kHz layer at 1.5 kHz
static void nudgePitch() {
    const double sr = 48000; const int64_t L = 96000;
    Sim s(sr, 120, 64); s.in = [&](int64_t n) { return n < L ? (float) (0.1 * std::sin(2 * M_PI * 1000.0 * (double) n / sr)) : 0.0f; };
    s.p.bars = 1; s.p.rec = true; s.run(L); s.p.rec = false; s.run(9600);
    s.p.nudge = 0.5f; s.run(9600);
    const size_t a = s.out.size(); s.run(24000);
    const double f = freq(s.out, a, s.out.size(), sr);
    CHECK(std::fabs(f / 1500.0 - 1.0) < 0.002, "pitch: %.2f Hz at nudge +0.5, want 1500 +- 0.2 %%", f);
}

// t82_rec: recording at half speed writes the input squeezed: 1 kHz in plays back at 2 kHz at 1x
static void nudgeRec() {
    const double sr = 48000;
    Sim s(sr, 120, 64); s.in = [&](int64_t n) { return (float) (0.1 * std::sin(2 * M_PI * 1000.0 * (double) n / sr)); };
    s.p.bars = 1; s.p.nudge = -0.5f; s.run(14400);
    s.p.rec = true; s.run(24000); s.p.rec = false; s.p.nudge = 0.0f;
    s.in = [](int64_t) { return 0.0f; }; s.run(96000 * 2);
    const size_t a = s.out.size(); s.run(96000);
    const double f = freq(s.out, a, s.out.size(), sr);
    CHECK(std::fabs(f / 2000.0 - 1.0) < 0.01, "rec nudged: %.2f Hz played back at 1x, want 2000 +- 1 %%", f);
}

// t82_blocks: the same output at block sizes 1, 64 and 1000 with nudges on block boundaries common to all
static void nudgeBlocks() {
    std::vector<float> ref;
    for (int N : {1, 64, 1000}) {
        Sim s(44100, 97, N, 200.0); s.in = [](int64_t n) { return n < 112000 ? noise(n) : 0.0f; };
        s.p.bars = 1; s.p.rec = true; s.run(112000); s.p.rec = false; s.run(64000);
        s.p.nudge = 0.7f; s.run(64000); s.p.nudge = -0.9f; s.p.rec = true; s.run(64000); s.p.rec = false; s.p.nudge = 0.0f; s.run(256000);
        if (ref.empty()) ref = s.out;
        else { int d = 0; for (size_t i = 0; i < ref.size(); ++i) d += ref[i] != s.out[i]; CHECK(d == 0, "nudge blocks: N=%d differs from N=1 in %d samples", N, d); }
    }
}

// zero allocations after prepare(), with every action fired
static void allocation() {
    Sim s(48000, 120, 64); s.in = noise; s.p.bars = 1;
    s.out.reserve(20 * 48000); s.at.reserve(20 * 48000);
    allocs = 0; counting = true;
    s.p.rec = true; s.run(96000); s.p.rec = false; s.run(9600);
    s.p.rec = true; s.run(30000); s.p.rec = false; s.run(9600);
    s.trigger(&ReelParams::undo); s.trigger(&ReelParams::redo); s.trigger(&ReelParams::undo);
    s.p.bars = 4; s.p.rec = true; s.run(50000); s.trigger(&ReelParams::undo); s.p.rec = false; s.run(640);
    s.p.nudge = 0.8f; s.run(9600); s.p.nudge = -1.0f; s.run(9600); s.p.nudge = 0.0f; s.run(48000);
    s.pos += 77777; s.run(9600); s.playing = false; s.run(9600); s.playing = true; s.run(9600);
    s.trigger(&ReelParams::clear); s.run(96000);
    counting = false;
    CHECK(allocs == 0, "allocation: %ld allocations on the audio path", allocs);
    CHECK(s.r.probe(Reel::kLayers) == 0, "clear: %g layers left", s.r.probe(Reel::kLayers));
}

int main() {
    idle(); null(); nodrift(44100, 97, 64); nodrift(48000, 140, 64); seam(); layers(); mixed(); transport();
    blocks(); full(); tempo(); allocation();
    nudgeMotor(); nudgeBrake(); nudgePitch(); nudgeRec(); nudgeBlocks();
    std::printf(fails ? "reel tests: %d FAILED\n" : "reel tests: all passed\n", fails);
    return fails ? 1 : 0;
}
