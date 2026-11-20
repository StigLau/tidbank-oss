#pragma once
#include <cmath>
#include <complex>
namespace tidbank {
// BBD anti-alias / reconstruction filters: an analogue low-pass design, discretised with the bilinear transform
// into a cascade of first/second-order sections. Header-only, JUCE-free, no allocation (fixed arrays).
// Designs: Butterworth (order, fc), or Sallen-Key sections with equal R from Raffel & Smith (DAFx-10) section 2.3:
//   2nd order: H = (1/(R^2 C1 C2)) / (s^2 + (2/(R C1)) s + 1/(R^2 C1 C2))
//   3rd order: H = a0 / (s^3 + a2 s^2 + a1 s + a0), a0 = 1/(R^3 C1 C2 C3), a1 = (C1 + 3 C3)/(R^2 C1 C2 C3), a2 = 2/(R C1) + 2/(R C2)
//   (C1 to ground, C2 feedback to the output, C3 to ground at the op-amp input; unity-gain buffer.)
// RS10 prints a1 = 1/(R^2 C1 C2) + 1/(R^2 C2 C3) = (C1 + C3)/(R^2 C1 C2 C3); nodal analysis of the circuit gives
// (C1 + 3 C3). With the corrected a1 the peaks match RS10 Fig. 5 (4.4 vs ~4.5 dB, 6.1 vs ~6 dB); the printed one gives
// 5.8 and 11.8 dB (bbd-research.md section 6 item 4, resolved 2026-11-16).
struct FilterDesign {
    enum Kind { None, Butterworth, SallenKey };
    Kind kind = None;
    int order = 0;          // Butterworth
    double fcHz = 0.0;      // Butterworth -3 dB
    double r = 10e3;        // Sallen-Key: equal R
    int nSk = 0;            // Sallen-Key: number of sections (each 2 or 3 caps)
    struct Sk { int n; double c[3]; } sk[3] = {};
    static FilterDesign butterworth(int order, double fcHz) { FilterDesign d; d.kind = Butterworth; d.order = order; d.fcHz = fcHz; return d; }
    FilterDesign& add2(double c1, double c2) { kind = SallenKey; sk[nSk++] = {2, {c1, c2, 0.0}}; return *this; }
    FilterDesign& add3(double c1, double c2, double c3) { kind = SallenKey; sk[nSk++] = {3, {c1, c2, c3}}; return *this; }
};

class SosCascade {
public:
    static constexpr int kMax = 6;
    // Designs at sample rate fs. Every section has unity DC gain.
    void design(const FilterDesign& d, double fs) {
        n = 0; reset();
        const double pi = 3.14159265358979323846;
        if (d.kind == FilterDesign::Butterworth) {
            const double wc = 2.0 * pi * d.fcHz, K = wc / std::tan(wc / (2.0 * fs));  // prewarped: fc exact
            for (int k = 1; k <= d.order / 2; ++k) {
                const double q = 1.0 / (2.0 * std::cos((2.0 * k - 1.0) * pi / (2.0 * d.order)));
                add2(1.0, wc / q, wc * wc, K);
            }
            if (d.order % 2) add1(1.0, wc, K);
        } else if (d.kind == FilterDesign::SallenKey) {
            const double K = 2.0 * fs, R = d.r;
            for (int i = 0; i < d.nSk; ++i) {
                const auto& s = d.sk[i];
                if (s.n == 2) {
                    add2(1.0, 2.0 / (R * s.c[0]), 1.0 / (R * R * s.c[0] * s.c[1]), K);
                } else {
                    const double c0 = 1.0 / (R * R * R * s.c[0] * s.c[1] * s.c[2]);
                    const double c1 = (s.c[0] + 3.0 * s.c[2]) / (R * R * s.c[0] * s.c[1] * s.c[2]);
                    const double c2 = 2.0 / (R * s.c[0]) + 2.0 / (R * s.c[1]);
                    // Real root by bisection on [-M, 0] (Cauchy bound; f(0) = c0 > 0), then (s - r)(s^2 + p s + q).
                    const double M = 1.0 + std::fmax(c0, std::fmax(c1, c2));
                    double lo = -M, hi = 0.0;
                    auto f = [&](double x) { return ((x + c2) * x + c1) * x + c0; };
                    for (int it = 0; it < 200; ++it) { const double m = 0.5 * (lo + hi); (f(m) > 0.0 ? hi : lo) = m; }
                    const double rt = 0.5 * (lo + hi), p = c2 + rt, q = c1 + rt * p;
                    add1(1.0, -rt, K);
                    add2(1.0, p, q, K);
                }
            }
        }
    }
    void reset() { for (auto& s : sec) s.z1 = s.z2 = 0.0; }
    double tick(double x) {
        for (int i = 0; i < n; ++i) {
            Sec& s = sec[i];
            const double y = s.b0 * x + s.z1;
            s.z1 = s.b1 * x - s.a1 * y + s.z2;
            s.z2 = s.b2 * x - s.a2 * y;
            if (std::fabs(s.z1) < 1e-15) s.z1 = 0.0;
            if (std::fabs(s.z2) < 1e-15) s.z2 = 0.0;
            x = y;
        }
        return x;
    }
    // |H| in dB at f (digital response at the design rate).
    double magDb(double f, double fs) const {
        const std::complex<double> z1 = std::polar(1.0, -2.0 * 3.14159265358979323846 * f / fs), z2 = z1 * z1;
        std::complex<double> h = 1.0;
        for (int i = 0; i < n; ++i) h *= (sec[i].b0 + sec[i].b1 * z1 + sec[i].b2 * z2) / (1.0 + sec[i].a1 * z1 + sec[i].a2 * z2);
        return 20.0 * std::log10(std::abs(h) + 1e-300);
    }
    int sections() const { return n; }
    bool stable() const {  // second-order: |a2| < 1 and |a1| < 1 + a2
        for (int i = 0; i < n; ++i) if (!(std::fabs(sec[i].a2) < 1.0 && std::fabs(sec[i].a1) < 1.0 + sec[i].a2)) return false;
        return true;
    }
private:
    struct Sec { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; } sec[kMax];
    int n = 0;
    // a0 / (a2 s^2 + a1 s + a0), s = K (1 - z^-1) / (1 + z^-1)
    void add2(double a2, double a1, double a0, double K) {
        if (n >= kMax) return;
        const double A = a2 * K * K, B = a1 * K, d0 = A + B + a0;
        sec[n++] = {a0 / d0, 2.0 * a0 / d0, a0 / d0, (2.0 * a0 - 2.0 * A) / d0, (A - B + a0) / d0};
    }
    // a0 / (a1 s + a0)
    void add1(double a1, double a0, double K) {
        if (n >= kMax) return;
        const double d0 = a1 * K + a0;
        sec[n++] = {a0 / d0, a0 / d0, 0.0, (a0 - a1 * K) / d0, 0.0};
    }
};
}  // namespace tidbank
