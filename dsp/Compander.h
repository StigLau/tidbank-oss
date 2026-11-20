#pragma once
#include <cmath>
namespace tidbank {
// NE570/571-style 2:1 compander (bbd-research.md 4, 8.6). Rectifier = average of |x| through a one-pole low-pass
// with tau = 10 kOhm x C_RECT (SA571 datasheet; Huovilainen DAFx-05 eqs. 26-27; Raffel & Smith DAFx-10).
//   compress: y = x * ref / avg|y|   (gain cell in the feedback path: output level = sqrt(input level * ref))
//   expand:   y = x * avg|x| / ref   (output level = input level^2 / ref)
// A steady sine through compress -> expand comes back at its own level; a filter between them is doubled in dB.
// Both gain cells stop at the same knee (floorAvg = ref at -maxGainDb): below it the compressor holds +maxGainDb and the
// expander -maxGainDb, so a steady quiet signal passes compress -> expand at unity (no filter between). Without the
// expander floor, a 2.2 ms rectifier emptied between echoes, each echo met up to +40 dB compression the expander did not
// undo, and m8192 at Intensity 0.95 settled into a tone near the knee at short delays. Side effect: BBD noise (added
// between the cells) now passes the expander at the floor gain instead of being gated, so idle hiss is ~36 dB higher (still
// below -100 dBFS) and m8192 (no filter loss) at Intensity >= ~1.05 builds self-oscillation from noise with no input.
// Header-only, no allocation. State in double, flushed below 1e-15.
class Compander {
public:
    void prepare(double sampleRate, double tauMs, double refLevel, double maxGainDb = 40.0) {
        a = std::exp(-1.0 / (tauMs * 1e-3 * sampleRate));
        ref = refLevel;
        floorAvg = ref * std::pow(10.0, -maxGainDb / 20.0);
        reset();
    }
    // Rectifiers start at the average of |x| of a sine of amplitude ref: compressor gain pi/2, expander gain 2/pi, product 1.
    void reset() { avgC = avgE = ref * 2.0 / 3.14159265358979323846; }
    double compress(double x) {
        const double y = x * ref / std::fmax(avgC, floorAvg);
        avgC = a * avgC + (1.0 - a) * std::fabs(y);
        if (avgC < 1e-15) avgC = 0.0;
        return y;
    }
    double expand(double x) {
        avgE = a * avgE + (1.0 - a) * std::fabs(x);
        if (avgE < 1e-15) avgE = 0.0;
        return x * std::fmax(avgE, floorAvg) / ref;
    }
    double compAvg() const { return avgC; }
    double expAvg() const { return avgE; }
private:
    double a = 0.0, ref = 0.5, floorAvg = 0.005, avgC = 0.0, avgE = 0.0;
};
}  // namespace tidbank
