#pragma once
#include <vector>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
namespace tidbank {
// Circular buffer with 4-point Hermite (read) and 32-tap windowed-sinc (readSinc) fractional reads.
// Allocate in prepare(), never on the audio thread.
class DelayLine {
public:
    // maxSamples = largest delay ever read (read or readSinc). Slack of kSincTaps+8 covers the sinc reach.
    void prepare(size_t maxSamples) { buf.assign(maxSamples + kSincTaps + 8, 0.0f); w = 0; buildSinc(); }
    // Zeroes the buffer in place (no allocation): safe on the audio thread, unlike prepare().
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); w = 0; }
    void push(float x) { buf[w] = x; w = (w + 1) % buf.size(); }
    // read(delay): delay in samples counted back from the last pushed sample.
    //   read(0) == last pushed sample, read(1) == the one before, read(d) interpolates between neighbours.
    //   Fractional delays need d >= 1 (the 4-point Hermite window reaches one sample newer than the floor).
    //   Push-then-read: echo delay == d. Read-then-push (feedback loops): the sample pushed this step
    //   is not yet in the buffer, so the loop delay is read(d) + 1; pass d - 1 to get d.
    float read(double delay) const {  // double: float loses ~0.03 samples of fraction at 6 s
        const size_t n = buf.size();
        const double pos = (double) w - 1.0 - delay;
        const float f = (float) (pos - std::floor(pos));
        long i = (long) std::floor(pos);
        auto at = [&](long k) { k %= (long) n; if (k < 0) k += (long) n; return buf[(size_t) k]; };
        const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
        const float c0 = y1, c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    // readSinc(delay): same delay semantics as read(), but a windowed-sinc FIR (32 taps, Kaiser beta 8.6, cutoff
    //   0.45*sr) taken from a polyphase table of 256 phases (+1 guard row), linearly interpolated between rows;
    //   every row has unity DC gain. Taps run from floor(pos)-15 to floor(pos)+16, so REQUIRES delay >= 17
    //   (callers guarantee it; no check here). No allocation. Not an identity at integer delays (passband is
    //   flat to ~1e-4 up to ~15 kHz at 48 kHz, so ~0.001 dB, and zero phase).
    float readSinc(double delay) const {
        const long n = (long) buf.size();
        const double pos = (double) w - 1.0 - delay;
        const double fl = std::floor(pos);
        const double q = (pos - fl) * (double) kPhases;
        long p = (long) q; if (p > (long) kPhases - 1) p = (long) kPhases - 1;
        const float a = (float) (q - (double) p);
        const float* r0 = &sinc[(size_t) p * kSincTaps];
        const float* r1 = r0 + kSincTaps;
        long idx = (long) fl - (kSincTaps / 2 - 1);
        idx %= n; if (idx < 0) idx += n;
        float s0 = 0.0f, s1 = 0.0f;
        for (int k = 0; k < kSincTaps; ++k) {
            const float v = buf[(size_t) idx];
            s0 += r0[k] * v; s1 += r1[k] * v;
            if (++idx == n) idx = 0;
        }
        return s0 + a * (s1 - s0);
    }
private:
    static constexpr int kSincTaps = 32, kPhases = 256;
    static double bessI0(double x) {
        double s = 1.0, t = 1.0; const double q = 0.25 * x * x;
        for (int k = 1; k < 60; ++k) { t *= q / ((double) k * k); s += t; }
        return s;
    }
    void buildSinc() {  // row p: fractional position p/256, taps k=-15..16 at t = k - p/256
        const double beta = 8.6, fc = 0.45, half = kSincTaps / 2.0, i0b = bessI0(beta);
        sinc.assign((size_t) (kPhases + 1) * kSincTaps, 0.0f);
        for (int p = 0; p <= kPhases; ++p) {
            double row[kSincTaps], sum = 0.0;
            for (int k = 0; k < kSincTaps; ++k) {
                const double t = (double) (k - (kSincTaps / 2 - 1)) - (double) p / kPhases;
                const double u = t / half;
                const double win = std::fabs(u) < 1.0 ? bessI0(beta * std::sqrt(1.0 - u * u)) / i0b : 0.0;
                const double x = 2.0 * M_PI * fc * t;
                row[k] = (std::fabs(x) < 1e-12 ? 2.0 * fc : 2.0 * fc * std::sin(x) / x) * win;
                sum += row[k];
            }
            for (int k = 0; k < kSincTaps; ++k) sinc[(size_t) (p * kSincTaps + k)] = (float) (row[k] / sum);
        }
    }
    std::vector<float> buf, sinc; size_t w = 0;
};
}
