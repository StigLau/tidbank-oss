#pragma once
// Reels v0 core (d26, d27; docs/plan-looper.md L0, L1, L3): a reel of layers locked to the host bar grid.
// No echo inside (d26): the Reels plugin outputs input + reel, and TidBank sits after it in Live. No Print, no varispeed yet:
// a layer recorded at another tempo is muted (flag tempoMuted) until the tempo is back.
//
// Timing (plan 2.2): a layer of `beats` at `bpmRec` is L = beats * 60 / bpmRec * sr samples, usually fractional. Its pass k starts at
// host sample round(k * L) counted from ppq 0, so every pass starts within 0.5 sample of the bar and the error never accumulates.
// A layer holds len = ceil(L) cells; cell m is the loop at time m, so the stored loop wraps from cell len - 1 to cell 0 after
// L - (len - 1) <= 1 sample. A pass is P = len or len - 1 host samples, never exactly L, so read and write positions r run raw (r = i,
// sample i of the pass) except in the last kWarp samples, where r ramps to reach L exactly at the next pass start:
// r = i + (L - P) (i - (P - kWarp)) / kWarp. The loop content stays continuous across every seam (no click) and the speed changes by
// at most 1/kWarp for kWarp samples. Raw cells are read and written as they are: a recorded layer plays back bit-exact, and with an
// integer L (120 BPM at 48 kHz) there is no ramp at all.
//
// Nudge: a hand on the reel pushes its speed to 1 + nudge (0x..2x); on release the motor slews it back to 1x. Only the
// speed returns, never the phase: the reel keeps the offset it gained (grid option (b)); Clear resets it. Reel time = host S + nOff + frac.
// While frac != 0 every read and write is interpolated; once the motor rests, frac glides to a whole sample (<= 1/256 speed change)
// and playback is raw again. Playback EQ: the reel output passes y = x - (1 - v) LP_50Hz(x), the net response
// (s + v wt)/(s + wt) of a NAB 3180 us playback EQ run at speed v (spd); the lows fade as the reel slows. Exactly y = x at v = 1.
//
// Header-only, JUCE-free. prepare() is the only allocation; everything else is index arithmetic (no allocation, memset or memmove).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tidbank {

struct ReelHost {
    double bpm = 120.0;
    bool playing = true;
    bool hasPpq = false;  // false: internal position from sample 0 (Standalone without a host transport, tests)
    double ppq = 0.0;     // quarter notes at the first sample of the block
};

struct ReelParams {
    int bars = 4;          // length of the NEXT layer, 4/4
    bool rec = false;      // state: recording while on (and the transport plays)
    bool undo = false, redo = false, clear = false;  // triggers on 0 -> 1
    float nudge = 0.0f;    // -1..+1, hand on the reel: speed target 1 + nudge (0x..2x); 0 = hand off, the motor returns to 1x
};

class Reel {
public:
    static constexpr int kMaxLayers = 16, kMaxCh = 2, kWarp = 64;
    static constexpr double kFadeSec = 0.005, kRunFadeSec = 0.010;
    // Nudge motor (2026-11-18): the hand follows in 60 ms, the motor returns in 400 ms (99 %, critically damped, as
    // TapeTransport). Playback EQ low corner: NAB 3180 us (50 Hz).
    static constexpr double kHandMs = 60.0, kMotorMs = 400.0, kSettle = 1.0 / 256.0, kEqTau = 3180e-6;
    enum Probe { kLayers, kHeard, kRecording, kFull, kTempoMuted, kSplices, kPoolUsed, kRecBlocked, kRunning, kSpeed, kOffset };

    void prepare(double sampleRate, double poolSeconds, int channels) {
        sr = sampleRate; ch = std::min(std::max(channels, 1), kMaxCh);
        poolLen = (size_t) std::max(1.0, std::floor(poolSeconds * sr));
        for (int c = 0; c < kMaxCh; ++c) pool[c].assign(c < ch ? poolLen : 0, 0.0f);
        fadeStep = 1.0 / std::max(1.0, kFadeSec * sr);
        runStep = 1.0 / std::max(1.0, kRunFadeSec * sr);
        aHand = std::exp(-6.638 / (kHandMs * 1e-3 * sr)); aMotor = std::exp(-6.638 / (kMotorMs * 1e-3 * sr));
        aEq = std::exp(-1.0 / (kEqTau * sr));
        clearAll(); nOff = 0; frac = 0.0; restMotor();
        prev = ReelParams(); first = true; runGain = 0.0; run = Stopped; splices = 0; internalS = 0;
    }
    // Audio-thread safe: closes a recording, keeps the layers (Panic in the plan: the reel is kept).
    void reset() { closeRecording(); run = Stopped; runGain = 0.0; first = true; restMotor(); }

    // Once per block, before the samples. Detects triggers and transport jumps.
    void setBlock(const ReelHost& h, const ReelParams& p) {
        host = h; params = p;
        bpm = std::max(h.bpm, 1.0);
        tempoChanged = !first && std::fabs(bpm - lastBpm) > 1e-9 * bpm;  // any change: v0 has no varispeed
        lastBpm = bpm;
        if (tempoChanged && rec.on) beginPunchOut();
        for (int j = 0; j < count; ++j) layers[j].tempoOk = std::fabs(layers[j].bpmRec - bpm) <= 1e-6 * bpm;

        if (p.clear && !prev.clear) { clearing = true; closeRecording(); }
        if (p.undo && !prev.undo) undo();
        if (p.redo && !prev.redo) redo();
        if (!p.rec) recBlocked = false;  // a refused or cut recording waits for Rec to be released
        prev = p;

        // Position at the block start, in host samples from ppq 0.
        blockN = 0;
        const int64_t S0 = h.hasPpq ? (int64_t) std::llround(h.ppq * 60.0 / bpm * sr) : internalS;
        if (!h.playing) {
            if (run == Running || run == Starting) { run = Stopping; jumpPending = false; beginPunchOut(); }
        } else if (run == Stopped || first) {
            sync(S0); run = Starting;
        } else if (run == Stopping) {
            pendingS = S0; jumpPending = true;  // playing again while fading out: finish the fade, then jump
        } else if (!tempoChanged && S0 != S && phaseKept(S0)) {
            sync(S0);  // a host loop of whole layer lengths (4 bars around a 1-bar layer): same place in every layer, no fade
        } else if (tempoChanged || std::llabs(S0 - S) > 2) {
            // locate, host loop or tempo change: fade out where we are, jump, fade in; a recording is closed
            pendingS = S0; jumpPending = true; run = Stopping; beginPunchOut();
            if (!tempoChanged) ++splices;
        } else if (S0 != S) sync(S0);  // one or two samples of host jitter: follow without a fade
        first = false;
    }

    // One sample. in/out have `channels` entries; out = the reel only (the wrapper adds the input).
    void process(const float* in, float* out) {
        for (int c = 0; c < ch; ++c) out[c] = 0.0f;
        advanceRun();
        ++blockN;
        const bool rolling = run != Stopped;
        if (rolling) {
            mix(out);
            if (params.rec && !rec.on && !recBlocked && (run == Running || run == Starting) && !clearing) openRecording();
            if (rec.on && !params.rec && !rec.punchingOut) beginPunchOut();
            if (rec.on) write(in);
            step();
        }
        if (clearing && (runGain == 0.0 || run == Stopped || allSilent())) finishClear();
    }

    double probe(int which) const {
        switch (which) {
            case kLayers: return count;
            case kHeard: return heard;
            case kRecording: return rec.on ? 1.0 : 0.0;
            case kFull: return full ? 1.0 : 0.0;
            case kTempoMuted: { int n = 0; for (int j = 0; j < heard; ++j) n += layers[j].tempoOk ? 0 : 1; return n; }
            case kSplices: return (double) splices;
            case kPoolUsed: return poolLen ? (double) top / (double) poolLen : 0.0;
            case kRecBlocked: return recBlocked ? 1.0 : 0.0;
            case kRunning: return run == Running ? 1.0 : 0.0;
            case kSpeed: return spd;
            case kOffset: return (double) nOff + frac;
        }
        return 0.0;
    }
    // Host sample position of the next sample (tests).
    int64_t position() const { return S; }

private:
    struct Layer {
        size_t offset = 0, len = 0;     // cells in the pool
        double L = 0.0;                 // exact loop length in samples
        double beats = 0.0, bpmRec = 120.0;
        size_t spanStart = 0, spanLen = 0;  // written cells (wraps)
        bool tempoOk = true;
        double gain = 0.0;
        // position: pass k starts at round(k L); i = sample index in the pass; P = pass length (len or len - 1)
        int64_t k = 0, i = 0, P = 0;
        int64_t passStart(int64_t kk) const { return (int64_t) std::llround((double) kk * L); }
        void locate(int64_t s) {
            k = (int64_t) std::floor((double) s / L);
            while (passStart(k + 1) <= s) ++k;
            while (passStart(k) > s) --k;
            i = s - passStart(k); P = passStart(k + 1) - passStart(k);
        }
        // read/write position r in cells, 0 <= r < L (fractional only in the last kWarp samples of a pass)
        double cellPos() const {
            const int64_t w0 = P - kWarp;
            if (i < w0) return (double) i;
            return (double) i + (L - (double) P) * (double) (i - w0) / (double) kWarp;
        }
        // value at position r for one channel: linear between cells; past cell len - 1 the next point is cell 0 at time L
        double at(const float* p, double r) const {
            const size_t m0 = std::min((size_t) r, len - 1);
            const double fr = r - (double) m0;
            double v = written(m0) ? (double) p[m0] : 0.0;
            if (fr > 0.0) {
                const bool wrap = m0 + 1 >= len;
                const size_t m1 = wrap ? 0 : m0 + 1;
                const double w = wrap ? fr / (L - (double) (len - 1)) : fr;
                const double v1 = written(m1) ? (double) p[m1] : 0.0;
                v += w * (v1 - v);
            }
            return v;
        }
        bool written(size_t m) const { return ((m + len - spanStart) % len) < spanLen; }
    };
    struct Recording {
        bool on = false, punchingOut = false;
        int layer = -1;
        double fade = 0.0;       // write gain: 0 -> 1 at punch in, 1 -> 0 at punch out
        double rPrev = 0.0;      // position of the previous written sample (a smaller r = a new pass)
        float xPrev[kMaxCh] = {0, 0};
        size_t written = 0;      // cells written into this layer; after len cells, writes add (overdub in the same layer)
    };
    enum Run { Stopped, Starting, Running, Stopping };

    void clearAll() { count = heard = 0; top = 0; full = false; rec = Recording(); recBlocked = false; clearing = false; }
    void finishClear() { clearAll(); clearing = false; recBlocked = params.rec; nOff = 0; frac = 0.0; }  // empty reel: back on the grid
    void restMotor() { spd = spd1 = 1.0; for (double& e : eqLp) e = 0.0; }
    // Read/write position of a layer in reel time: the grid position of S + nOff, plus the sub-sample offset, wrapped at L.
    double pos(const Layer& l) const { const double r = l.cellPos() + frac; return r >= l.L ? r - l.L : r; }
    bool allSilent() const { for (int j = 0; j < count; ++j) if (layers[j].gain > 0.0) return false; return true; }

    bool phaseKept(int64_t s) const {
        for (int j = 0; j < count; ++j) { Layer t = layers[j]; t.locate(s + nOff); if (t.i != layers[j].i) return false; }
        return true;
    }
    void sync(int64_t s) { S = s; for (int j = 0; j < count; ++j) layers[j].locate(S + nOff); }

    void advanceRun() {
        switch (run) {
            case Starting: runGain = std::min(1.0, runGain + runStep); if (runGain >= 1.0) run = Running; break;
            case Stopping:
                runGain = std::max(0.0, runGain - runStep);
                if (runGain <= 0.0) {
                    if (jumpPending) { jumpPending = false; sync(pendingS + blockN); run = Starting; }
                    else run = Stopped;
                }
                break;
            case Stopped: case Running: break;
        }
    }

    // Undo/redo fade the layer gains; nothing is deleted until a new recording reuses the memory.
    void undo() {
        if (rec.on) { cutRecording(); return; }  // undo during a take: drop the take
        if (heard > 0) --heard;
    }
    void redo() { if (!rec.on && heard < count) ++heard; }

    void openRecording() {
        // a new take drops the undone layers above `heard` and reuses their memory
        if (heard < count) { count = heard; top = count > 0 ? layers[count - 1].offset + layers[count - 1].len : 0; }
        Layer l;
        l.beats = 4.0 * std::max(params.bars, 1); l.bpmRec = bpm;
        l.L = l.beats * 60.0 / bpm * sr;
        l.len = (size_t) std::ceil(l.L - 1e-9);
        if (count >= kMaxLayers || l.len + top > poolLen || l.len < 2 * (size_t) kWarp) { full = true; recBlocked = true; return; }
        full = false;
        l.offset = top; l.gain = 0.0; l.locate(S + nOff);
        l.spanStart = std::min((size_t) std::floor(pos(l)), l.len - 1); l.spanLen = 0;
        top += l.len;
        layers[count] = l; rec = Recording(); rec.on = true; rec.layer = count;
        rec.rPrev = pos(l) - 1.0;
        ++count; heard = count;
    }
    void beginPunchOut() { if (rec.on) rec.punchingOut = true; }
    void closeRecording() { rec.on = false; rec.punchingOut = false; rec.layer = -1; }
    void cutRecording() {  // undo during a take: stop writing and drop the layer (its gain fades out)
        closeRecording();
        if (heard > 0) --heard;
        recBlocked = true;
    }

    void write(const float* in) {
        Layer& l = layers[rec.layer];
        if (rec.punchingOut) rec.fade = std::max(0.0, rec.fade - fadeStep);
        else rec.fade = std::min(1.0, rec.fade + fadeStep);
        const double r = pos(l);
        float x[kMaxCh];
        for (int c = 0; c < ch; ++c) x[c] = in[c] * (float) rec.fade;
        if (r < rec.rPrev) {  // new pass: finish the cells before L (the next point, at L, is this sample), then start at 0
            fill(l, rec.rPrev, l.L, x, true);
            rec.rPrev -= l.L;  // -1 on the grid; the true previous time when nudged
            for (int c = 0; c < ch; ++c) rec.xPrev[c] = x[c];
        }
        fill(l, rec.rPrev, r, x, false);
        rec.rPrev = r;
        for (int c = 0; c < ch; ++c) rec.xPrev[c] = x[c];
        if (rec.punchingOut && rec.fade <= 0.0) closeRecording();
    }
    // Cells m with r0 < m <= r1 (m < r1 when `open`), linear between (r0, xPrev) and (r1, x). Raw when r1 - r0 == 1.
    void fill(Layer& l, double r0, double r1, const float* x, bool open) {
        const double dr = r1 - r0;
        for (double m = std::floor(r0) + 1.0; open ? m < r1 : m <= r1; m += 1.0) {
            if (m < 0.0 || m > (double) (l.len - 1)) continue;
            const size_t cell = (size_t) m;
            const double a = dr > 0.0 ? (m - r0) / dr : 1.0;
            const bool add = rec.written >= l.len;
            for (int c = 0; c < ch; ++c) {
                const float v = a == 1.0 ? x[c] : (float) (rec.xPrev[c] + a * (x[c] - rec.xPrev[c]));
                float& dst = pool[c][l.offset + cell];
                dst = add ? dst + v : v;
            }
            ++rec.written;
            if (l.spanLen < l.len) ++l.spanLen;
        }
    }

    // Sum of the heard layers, each with its undo/redo/tempo fade, then the playback EQ at reel speed v.
    void mix(float* out) {
        const double gStep = fadeStep;
        for (int j = count - 1; j >= 0; --j) {
            Layer& l = layers[j];
            const double target = (j < heard && l.tempoOk && !clearing) ? 1.0 : 0.0;
            l.gain = l.gain < target ? std::min(target, l.gain + gStep) : std::max(target, l.gain - gStep);
            if (l.gain <= 0.0) continue;
            const double r = pos(l);
            const double gl = l.gain * runGain;
            for (int c = 0; c < ch; ++c) out[c] += (float) (gl * l.at(&pool[c][l.offset], r));
        }
        for (int c = 0; c < ch; ++c) {
            const double x = out[c];
            eqLp[c] = x + aEq * (eqLp[c] - x);
            if (std::fabs(eqLp[c]) < 1e-15) eqLp[c] = 0.0;
            if (spd != 1.0) out[c] = (float) (x - (1.0 - spd) * eqLp[c]);
        }
    }

    // Motor: true while the speed is off 1x (hand on, or slewing back). Two cascaded one-poles, critically damped.
    bool moveMotor() {
        const bool hand = std::fabs(params.nudge) > 1e-4f;
        if (!hand && spd == 1.0 && spd1 == 1.0) return false;
        const double tgt = hand ? 1.0 + std::min(std::max((double) params.nudge, -1.0), 1.0) : 1.0, a = hand ? aHand : aMotor;
        spd1 = tgt + a * (spd1 - tgt); spd = spd1 + a * (spd - spd1);
        spd = std::min(std::max(spd, 0.0), 2.0);
        if (!hand && std::fabs(spd1 - 1.0) < 1e-6 && std::fabs(spd - 1.0) < 1e-6) spd = spd1 = 1.0;
        return true;
    }

    // One host sample: the reel moves m = 0, 1 or 2 whole samples (speed 0x..2x) plus the change of frac.
    void step() {
        ++S; if (!host.hasPpq) internalS = S;
        int m = 1;
        if (moveMotor()) {
            const double t = frac + spd; m = (int) std::floor(t); frac = t - m;
        } else if (frac != 0.0) {  // motor at rest: glide to the nearest whole sample, then raw playback
            const double end = frac < 0.5 ? 0.0 : 1.0, d = end - frac;
            if (std::fabs(d) <= kSettle) { m = 1 + (int) end; frac = 0.0; }
            else { const double t = frac + 1.0 + (d > 0 ? kSettle : -kSettle); m = (int) std::floor(t); frac = t - m; }
        }
        nOff += m - 1;
        for (int j = 0; j < count; ++j) {
            Layer& l = layers[j];
            for (int n = 0; n < m; ++n)
                if (++l.i >= l.P) { ++l.k; l.i = 0; l.P = l.passStart(l.k + 1) - l.passStart(l.k); }
        }
    }

    double sr = 48000.0, fadeStep = 0.0, runStep = 0.0, bpm = 120.0, lastBpm = 0.0, runGain = 0.0;
    double spd = 1.0, spd1 = 1.0, frac = 0.0, aHand = 0.0, aMotor = 0.0, aEq = 0.0, eqLp[kMaxCh] = {0, 0};  // nudge
    int64_t nOff = 0;
    int ch = 2, count = 0, heard = 0;
    size_t poolLen = 0, top = 0;
    std::vector<float> pool[kMaxCh];
    Layer layers[kMaxLayers];
    Recording rec;
    ReelHost host; ReelParams params, prev;
    Run run = Stopped;
    int64_t S = 0, pendingS = 0, internalS = 0, blockN = 0;
    uint64_t splices = 0;
    bool first = true, tempoChanged = false, jumpPending = false, full = false, recBlocked = false, clearing = false;
};

}  // namespace tidbank
