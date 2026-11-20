#pragma once
// Debug probe: per-sample delay tracing of the tape heads and the BBD, block timing, CPU load.
// Compiled only with TIDBANK_DEBUG_PROBE (CMake option, default OFF). Without it this header is empty and the engines
// hold no probe code or members. JUCE-free: used by the ears renderer and the plugin. The probe only OBSERVES: it reads
// engine state, never writes it, so audio is bit-identical with the probe on or off (asserted by ears p_null_*).
//
// Audio thread (Hub::blockBegin/blockEnd/advance, Tracer::*): no allocation, no locks, no I/O. Records are fixed-size
// PODs pushed into a preallocated single-producer/single-consumer ring; a full ring drops the record and counts it.
// Consumer thread (plugin: a juce::Timer, renderer: the main thread after rendering): Hub::pop, formatCsv/formatLine.
#ifdef TIDBANK_DEBUG_PROBE
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace tidbank { namespace probe {

enum RecType : uint8_t { kDelay = 1, kBlock = 2, kCpu = 3 };

// One log record. Delay: a marked input sample n0 seen coming out of one tape head (head 1..3) or the BBD (head 0).
struct Rec {
    uint8_t type = 0, engine = 0, ch = 0, head = 0;  // engine 0 = tape, 1 = BBD
    int32_t cell = -1;                               // BBD: ring cell the marked value was written to
    char name[8] = {0};                              // "tape" or the BBD profile id
    int64_t n0 = 0;                                  // marked input sample index (call index of process())
    int64_t tick = -1;                               // BBD: clock tick count at which the value entered the ring
    int64_t nOutHost = 0;                            // host sample (call) in which the output was seen
    int64_t blkStart = 0, outBlkStart = 0;           // block holding n0 / holding the output (Block: this block)
    int32_t blkLen = 0, outBlkLen = 0;
    double nIn = 0.0;    // time the value entered: tape = n0, BBD = tick time (fractional host samples)
    double nOut = 0.0;   // time the output played the value, sub-sample
    double measured = 0.0, computed = 0.0;  // delay in samples: nOut - nIn, and the engine's own value at nOut
    double speed = 0.0, bpm = 0.0;          // tape: v; BBD: clock Hz; bpm in effect (set by the driver)
    double cpuAvg = 0.0, cpuPeak = 0.0;     // Cpu: percent of the block time; Block: this block (cpuAvg)
    uint64_t drops = 0;                     // Cpu: records dropped so far (ring full)
    uint32_t blocks = 0;                    // Cpu: blocks in the interval
};

// Lock-free SPSC ring of Rec; capacity fixed at prepare (power of two). Producer = audio thread, consumer = one other.
class Ring {
public:
    void allocate(size_t pow2) { if (buf.empty()) { buf.assign(pow2, Rec{}); mask = pow2 - 1; } }
    bool push(const Rec& r) {
        const size_t h = head.load(std::memory_order_relaxed), t = tail.load(std::memory_order_acquire);
        if (h - t >= buf.size()) { dropped.fetch_add(1, std::memory_order_relaxed); return false; }
        buf[h & mask] = r; head.store(h + 1, std::memory_order_release); return true;
    }
    bool pop(Rec& r) {
        const size_t t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false;
        r = buf[t & mask]; tail.store(t + 1, std::memory_order_release); return true;
    }
    uint64_t drops() const { return dropped.load(std::memory_order_relaxed); }
private:
    static_assert(std::atomic<size_t>::is_always_lock_free, "probe ring indices must be lock-free");
    std::vector<Rec> buf; size_t mask = 0;
    std::atomic<size_t> head { 0 }, tail { 0 };
    std::atomic<uint64_t> dropped { 0 };
};

// Shared by the driver (plugin processBlock / renderer loop) and the engine tracers. Audio-thread state is plain.
class Hub {
public:
    struct Config { double intervalSec = 1.0; double firstSec = -1.0; size_t capacity = 16384; };  // firstSec < 0: = interval
    // Message thread / setup. Allocates the ring once (a later prepare keeps it: the consumer may be draining).
    void prepare(double sampleRate, const Config& c) {
        sr = sampleRate;
        interval = std::max<int64_t>(1, std::llround(c.intervalSec * sr));
        first = std::max<int64_t>(0, std::llround((c.firstSec < 0.0 ? c.intervalSec : c.firstSec) * sr));
        size_t cap = 1024; while (cap < c.capacity) cap <<= 1;
        ring.allocate(cap);
        n = 0; inFlight = 0; blkStart = 0; blkLen = 0; nextCpu = interval; resetCpu();
    }
    // ---- audio thread
    void blockBegin(int numSamples) {
        blkStart = n; blkLen = numSamples; flightAtBegin = inFlight > 0; t0 = Clock::now();
    }
    void blockEnd() {
        const double dt = std::chrono::duration<double>(Clock::now() - t0).count();
        const double pct = blkLen > 0 ? 100.0 * dt / ((double) blkLen / sr) : 0.0;
        cpuSum += pct; cpuPeak = std::max(cpuPeak, pct); ++cpuBlocks;
        if (flightAtBegin || inFlight > 0) {
            Rec r; r.type = kBlock; r.blkStart = blkStart; r.blkLen = blkLen; r.cpuAvg = pct; r.nOutHost = n; r.bpm = bpm; push(r);
        }
        if (n >= nextCpu) {  // blockBegin..End cover whole blocks, so n is the block end here (driver advanced it)
            Rec r; r.type = kCpu; r.nOutHost = n; r.cpuAvg = cpuSum / (double) std::max<uint32_t>(cpuBlocks, 1); r.cpuPeak = cpuPeak;
            r.blocks = cpuBlocks; r.drops = ring.drops(); r.bpm = bpm; push(r);
            while (nextCpu <= n) nextCpu += interval;
            resetCpu();
        }
    }
    void advance() { ++n; }  // after each processed sample
    bool push(const Rec& r) { return ring.push(r); }
    // ---- consumer
    bool pop(Rec& r) { return ring.pop(r); }
    uint64_t drops() const { return ring.drops(); }

    // Audio-thread state, read by the tracers.
    int64_t n = 0;           // index of the sample being processed (driver increments after each sample)
    int64_t blkStart = 0; int32_t blkLen = 0;
    double bpm = 120.0;      // bpm in effect; set by the driver
    double sr = 48000.0;
    int64_t interval = 48000, first = 48000;
    int inFlight = 0;        // marks not yet seen at an output (all tracers)
private:
    using Clock = std::chrono::steady_clock;
    void resetCpu() { cpuSum = 0.0; cpuPeak = 0.0; cpuBlocks = 0; }
    Ring ring; Clock::time_point t0{};
    double cpuSum = 0.0, cpuPeak = 0.0; uint32_t cpuBlocks = 0; int64_t nextCpu = 0; bool flightAtBegin = false;
};

// Per-engine delay tracer (one per mono engine). Marks input sample n0 = first + k * interval and reports when the
// engine's output plays it. Pending marks live in a fixed array (no allocation); a full array drops the new mark.
class Tracer {
public:
    void init(Hub* h, uint8_t engineId, const char* engineName) {
        hub = h; engine = engineId; std::memset(name, 0, sizeof name); std::strncpy(name, engineName, sizeof name - 1);
        nextMark = h->first;
        for (int k = 0; k < 3; ++k) { cnt[k] = 0; front[k] = 0; }  // hub counters were just reset by Hub::prepare
        havePrev = false; waiting = false; tickIdx = 0; skippedMarks = 0;
    }
    // Engine reset (tape erased): marks in flight are gone.
    void clear() {
        if (hub) for (int k = 0; k < 3; ++k) hub->inFlight -= cnt[k];
        for (int k = 0; k < 3; ++k) { cnt[k] = 0; front[k] = 0; }
        if (hub && waiting) --hub->inFlight;
        havePrev = false; waiting = false; tickIdx = 0; skippedMarks = 0;
    }
    uint64_t skipped() const { return skippedMarks; }
    int64_t now() const { return hub->n; }  // host sample (call) being processed

    // Tape: call once per sample after tau/gain are solved. tau_i in samples; the head plays input time n - tau_i.
    void tapeStep(const double* tau, const double* gain, double v) {
        const int64_t n = hub->n;
        scheduleMark(n, [&]() { for (int k = 0; k < 3; ++k) addPend(k, n, 0, 0.0); });
        double q[3];
        for (int k = 0; k < 3; ++k) q[k] = (double) n - tau[k];
        if (havePrev) {
            for (int k = 0; k < 3; ++k) {
                while (cnt[k] > 0 && q[k] >= (double) pend[k][front[k]].n0) {
                    const Pend& p = pend[k][front[k]];
                    const double den = q[k] - prevQ[k];
                    double f = den > 1e-12 ? ((double) p.n0 - prevQ[k]) / den : 1.0;  // q grows <= ... per sample; f in [0,1]
                    f = std::min(std::max(f, 0.0), 1.0);
                    if (gain[k] > 0.0) {
                        Rec r = base(kDelay); r.head = (uint8_t) (k + 1); r.n0 = p.n0; r.nIn = (double) p.n0;
                        r.nOut = (double) (n - 1) + f; r.nOutHost = n; r.measured = r.nOut - r.nIn;
                        r.computed = prevTau[k] + f * (tau[k] - prevTau[k]);
                        r.blkStart = p.blkStart; r.blkLen = p.blkLen; r.speed = v;
                        hub->push(r);
                    }
                    popFront(k);
                }
            }
        }
        for (int k = 0; k < 3; ++k) { prevQ[k] = q[k]; prevTau[k] = tau[k]; }
        havePrev = true;
    }

    // BBD: bbdBegin() once per process() call, bbdTick() at every clock tick (before the ring write advances).
    // A mark waits for the first tick at or after the start of call n0; that tick's cell holds the sampled value.
    void bbdBegin() {
        scheduleMark(hub->n, [&]() { waiting = true; waitN0 = hub->n; waitBlkStart = hub->blkStart; waitBlkLen = hub->blkLen; ++hub->inFlight; });
    }
    // tickTime: when the tick sampled the input, host samples (call index - 1 + fraction in (0, 1]).
    // cells: ring length; cell: index being written (and read) now; fclk: clock Hz; delayMs: the engine's probe(1).
    void bbdTick(double tickTime, int cells, int cell, double fclk, double delayMs) {
        if (waiting) {
            waiting = false;
            addPend(0, waitN0, cell, tickTime, tickIdx, waitBlkStart, waitBlkLen);  // a full array counts it as skipped
            --hub->inFlight;  // the wait slot gave way to the pend slot (addPend counted that one)
        }
        while (cnt[0] > 0 && tickIdx >= pend[0][front[0]].tick + cells) {
            const Pend& p = pend[0][front[0]];
            Rec r = base(kDelay); r.head = 0; r.n0 = p.n0; r.cell = p.cell; r.tick = p.tick; r.nIn = p.tIn;
            r.nOut = tickTime; r.nOutHost = hub->n; r.measured = tickTime - p.tIn;
            r.computed = delayMs * 1e-3 * hub->sr; r.speed = fclk; r.blkStart = p.blkStart; r.blkLen = p.blkLen;
            hub->push(r); popFront(0);
        }
        ++tickIdx;
    }

private:
    struct Pend { int64_t n0, tick; double tIn; int32_t cell; int64_t blkStart; int32_t blkLen; };
    static constexpr int kMaxPend = 64;

    template <class F> void scheduleMark(int64_t n, F make) {
        if (n < nextMark) return;
        if (n == nextMark) make();  // a schedule that fell behind (engine not stepped) is skipped, never back-dated
        while (nextMark <= n) nextMark += hub->interval;
    }
    bool addPend(int k, int64_t n0, int cell, double tIn, int64_t tick = -1, int64_t bs = -1, int32_t bl = 0) {
        if (bs < 0) { bs = hub->blkStart; bl = hub->blkLen; }
        if (cnt[k] >= kMaxPend) { ++skippedMarks; return false; }
        Pend& p = pend[k][(front[k] + cnt[k]) % kMaxPend];
        p = Pend { n0, tick, tIn, (int32_t) cell, bs, bl };
        ++cnt[k]; ++hub->inFlight; return true;
    }
    void popFront(int k) { front[k] = (front[k] + 1) % kMaxPend; --cnt[k]; --hub->inFlight; }
    Rec base(RecType t) const {
        Rec r; r.type = t; r.engine = engine; std::memcpy(r.name, name, sizeof name); r.bpm = hub->bpm;
        r.outBlkStart = hub->blkStart; r.outBlkLen = hub->blkLen; return r;
    }

    Hub* hub = nullptr; uint8_t engine = 0; char name[8] = {0};
    int64_t nextMark = 0; Pend pend[3][kMaxPend]; int cnt[3] = {0, 0, 0}, front[3] = {0, 0, 0};
    double prevQ[3] = {0, 0, 0}, prevTau[3] = {0, 0, 0}; bool havePrev = false;
    bool waiting = false; int64_t waitN0 = 0, waitBlkStart = 0; int32_t waitBlkLen = 0; int64_t tickIdx = 0;
    uint64_t skippedMarks = 0;
};

// ---- formatting (consumer thread only: snprintf)
inline const char* csvHeader() {
    return "type,engine,head,cell,tick,n0,n_in,n_out,out_host_sample,measured_samples,measured_ms,computed_samples,computed_ms,"
           "diff_samples,speed,bpm,in_blk_start,in_blk_len,out_blk_start,out_blk_len,cpu_avg_pct,cpu_peak_pct,blocks,drops";
}
inline int formatCsv(const Rec& r, double sr, char* b, size_t cap) {
    const double ms = 1e3 / sr;
    switch (r.type) {
    case kDelay:
        return std::snprintf(b, cap, "delay,%s,%d,%d,%lld,%lld,%.4f,%.4f,%lld,%.4f,%.5f,%.4f,%.5f,%.5f,%.6f,%.3f,%lld,%d,%lld,%d,,,,",
            r.name, r.head, r.cell, (long long) r.tick, (long long) r.n0, r.nIn, r.nOut, (long long) r.nOutHost, r.measured, r.measured * ms,
            r.computed, r.computed * ms, r.measured - r.computed, r.speed, r.bpm, (long long) r.blkStart, r.blkLen,
            (long long) r.outBlkStart, r.outBlkLen);
    case kBlock:
        return std::snprintf(b, cap, "block,,,,,,,,%lld,,,,,,,%.3f,%lld,%d,,,%.4f,,,", (long long) r.nOutHost, r.bpm, (long long) r.blkStart, r.blkLen, r.cpuAvg);
    default:
        return std::snprintf(b, cap, "cpu,,,,,,,,%lld,,,,,,,%.3f,,,,,%.4f,%.4f,%u,%llu", (long long) r.nOutHost, r.bpm, r.cpuAvg, r.cpuPeak,
            r.blocks, (unsigned long long) r.drops);
    }
}
// One readable line per record, for the plugin log.
inline int formatLine(const Rec& r, double sr, char* b, size_t cap) {
    const double ms = 1e3 / sr;
    switch (r.type) {
    case kDelay: {
        const double inOff = (double) (r.n0 - r.blkStart), outOff = r.nOut - (double) r.outBlkStart;
        const char* what = r.engine == 0 ? "head" : "out ";
        int k = std::snprintf(b, cap, "delay %-6s %s %d n0=%lld [blk %lld+%d @%.0f] -> out=%.3f [blk %lld+%d @%.2f]  measured=%.3f smp %.4f ms  "
            "computed=%.3f smp %.4f ms  diff=%+.4f smp  %s=%.5f bpm=%.2f",
            r.name, what, r.head, (long long) r.n0, (long long) r.blkStart, r.blkLen, inOff, r.nOut, (long long) r.outBlkStart, r.outBlkLen, outOff,
            r.measured, r.measured * ms, r.computed, r.computed * ms, r.measured - r.computed, r.engine == 0 ? "speed" : "clk_hz", r.speed, r.bpm);
        if (r.engine == 1 && k > 0 && (size_t) k < cap) k += std::snprintf(b + k, cap - (size_t) k, " cell=%d tick=%lld sampled@%.3f", r.cell, (long long) r.tick, r.nIn);
        return k;
    }
    case kBlock:
        return std::snprintf(b, cap, "block start=%lld len=%d cpu=%.2f%%", (long long) r.blkStart, r.blkLen, r.cpuAvg);
    default:
        return std::snprintf(b, cap, "cpu   @%lld avg=%.2f%% peak=%.2f%% blocks=%u drops=%llu", (long long) r.nOutHost, r.cpuAvg, r.cpuPeak,
            r.blocks, (unsigned long long) r.drops);
    }
}

// RAII for a processBlock-like scope: begin on entry, end on every exit path. The driver advances n per sample.
struct BlockScope {
    Hub* h;
    BlockScope(Hub* hub, int numSamples) : h(numSamples > 0 ? hub : nullptr) { if (h) h->blockBegin(numSamples); }
    ~BlockScope() { if (h) h->blockEnd(); }
};

}}  // namespace tidbank::probe
#endif  // TIDBANK_DEBUG_PROBE
