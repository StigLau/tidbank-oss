// tidbank_render: offline deterministic renderer. The agent's ears, step 1 (render). Step 2 is analyze.py.
// Usage: tidbank_render --out y.wav [--in x.wav | --stim impulse|clicks|tone|twotone|burst|noise] [--sr 48000] [--secs 4]
//        [--freq 1000 --level 0.5 (tone)] [--bpm 120 --div 0.5] [--delay-ms N] [--fb 0.5] [--dry 0|1] [--wet 0|1]
//   --div is in quarter-notes (0.5 = 1/8). Delay = div*60000/bpm unless --delay-ms given.
//   --stim twotone: --freq (1000) at --level (0.25) plus --freq2 (3450) at --level2 (0.25).
//   --stim burst: 1 kHz (--freq) bursts, 20 ms, 2 ms raised-cosine fades, first at 0.1 s, one per beat at --bpm.
//   --engine naive|tape (default naive)   --params k=v,k=v (TapeParams field names; also nudge,inertia,div,time aliases)
//   --sched "t:k=v,k=v;t:k=v" (seconds; applied at sample round(t*sr); naive honours bpm/division/fb)
//   --probe v|tau1|tau2|tau3|tauerr1..3|clamped (write engine.probe(i) per sample instead of audio; tape only)
//   --engine bbd --profile m8192|d4096|c8192: BBD engine. Params: time_ms (time), fb, bright 0|1, comp 0|1, noise 0|1, nonlin 0|1, droop 0|1 (d36).
//   --probe clk|bbd_ms (bbd): clock Hz / N/(2 f_clk) in ms per sample.
//   --dump-profile ID|all: print the profile(s) as JSON (designed filter response at --sr) and exit; no --out needed.
//   --write-in path: also write the stimulus (input) as WAV, for transfer-function analysis.
//   --probe-csv out.csv [--probe-interval 1] [--probe-offset-s T] [--probe-block 512]: debug probe, tape/bbd only, needs a
//     build with TIDBANK_DEBUG_PROBE (tidbank_render_probe): every --probe-interval seconds (first mark at --probe-offset-s, default = the
//     interval) mark an input sample and write when each tape head / the BBD plays it, plus block timing and CPU, as CSV. Audio is unchanged.
//   Unknown param/sched key or bad value: exit 2.
#include "NaiveEcho.h"
#include "TapeEngine.h"
#include "BbdEngine.h"
#ifdef TIDBANK_DEBUG_PROBE
#include "DebugProbe.h"
#endif
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cmath>
using namespace tidbank;

static void put16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void put32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static bool writeWav(const char* path, const std::vector<float>& x, int sr) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    uint32_t bytes = (uint32_t) (x.size() * 4);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16);
    put16(f, 3); put16(f, 1); put32(f, (uint32_t) sr); put32(f, (uint32_t) sr * 4); put16(f, 4); put16(f, 32);
    fwrite("data", 1, 4, f); put32(f, bytes); fwrite(x.data(), 4, x.size(), f); fclose(f); return true;
}
static bool readWav(const char* path, std::vector<float>& x, int& sr) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    char id[4]; uint32_t sz; fread(id, 1, 4, f); fread(&sz, 4, 1, f); fread(id, 1, 4, f);
    uint16_t fmt = 0, ch = 1, bits = 16; uint32_t rate = 48000;
    while (fread(id, 1, 4, f) == 4 && fread(&sz, 4, 1, f) == 1) {
        if (!memcmp(id, "fmt ", 4)) {
            fread(&fmt, 2, 1, f); fread(&ch, 2, 1, f); fread(&rate, 4, 1, f); fseek(f, 6, SEEK_CUR); fread(&bits, 2, 1, f); fseek(f, (long) sz - 16, SEEK_CUR);
        } else if (!memcmp(id, "data", 4)) {
            std::vector<uint8_t> raw(sz); fread(raw.data(), 1, sz, f);
            size_t frames = sz / (bits / 8) / ch; x.assign(frames, 0.0f);
            for (size_t i = 0; i < frames; ++i) { const uint8_t* p = &raw[i * ch * (bits / 8)];
                if (fmt == 3 && bits == 32) { float v; memcpy(&v, p, 4); x[i] = v; }
                else if (bits == 16) x[i] = (int16_t) (p[0] | p[1] << 8) / 32768.0f;
                else if (bits == 24) x[i] = ((int32_t) (p[0] << 8 | p[1] << 16 | p[2] << 24) >> 8) / 8388608.0f; }
            sr = (int) rate; fclose(f); return true;
        } else fseek(f, (long) sz, SEEK_CUR);
    }
    fclose(f); return false;
}

// Mutable view of one param set: TapeParams plus the naive engine's own values (kept as double so the naive path is bit-identical).
struct Live { TapeParams tp; double bpm, div, fb; bool delayMsGiven; BbdParams bp; };

static bool applyKey(Live& L, std::string k, const std::string& vs) {
    char* end = nullptr; const double v = strtod(vs.c_str(), &end);
    if (vs.empty() || *end) return false;
    static const std::map<std::string, std::string> alias = {{"nudge", "nudge_cents"}, {"inertia", "inertia_ms"}, {"div", "division"}, {"time", "time_ms"}};
    auto al = alias.find(k); if (al != alias.end()) k = al->second;
    TapeParams& p = L.tp;
    if (k == "sync") p.sync = (int) v; else if (k == "bpm") { p.bpm = (float) v; L.bpm = v; }
    else if (k == "division") { p.division = (float) v; L.div = v; }
    else if (k == "head_target") p.head_target = (int) v; else if (k == "spacing") p.spacing = (int) v;
    else if (k == "nudge_cents") p.nudge_cents = (float) v; else if (k == "time_ms") p.time_ms = (float) v;
    else if (k == "inertia_ms") p.inertia_ms = (float) v; else if (k == "mode") p.mode = (int) v;
    else if (k == "intensity") p.intensity = (float) v; else if (k == "fb_hpf") p.fb_hpf = (float) v;
    else if (k == "fb_lpf") p.fb_lpf = (float) v; else if (k == "drive") p.drive = (float) v;
    else if (k == "head") p.head = (int) v;
    else if (k == "fb") L.fb = v;  // naive engine feedback (ignored by tape), BBD feedback
    else if (k == "bright") L.bp.bright = (int) v; else if (k == "comp") L.bp.compander = (int) v;
    else if (k == "noise") L.bp.noise = (int) v; else if (k == "nonlin") L.bp.nonlin = (int) v;
    else if (k == "droop") L.bp.droopComp = (int) v;  // BBD hold droop: 0 authentic, 1 compensated (d36)
    else return false;
    L.bp.time_ms = p.time_ms; L.bp.feedback = (float) L.fb;
    return true;
}
static std::vector<std::string> split(const std::string& s, char c) {
    std::vector<std::string> r; size_t b = 0;
    for (;;) { size_t e = s.find(c, b); r.push_back(s.substr(b, e == std::string::npos ? e : e - b)); if (e == std::string::npos) break; b = e + 1; }
    return r;
}
static bool applyList(Live& L, const std::string& list) {
    for (const auto& kv : split(list, ',')) {
        if (kv.empty()) continue;
        const size_t eq = kv.find('=');
        if (eq == std::string::npos || !applyKey(L, kv.substr(0, eq), kv.substr(eq + 1))) { fprintf(stderr, "bad param '%s'\n", kv.c_str()); return false; }
    }
    return true;
}
struct SchedEv { size_t at; std::string list; };

static void dumpFact(const char* k, const Fact& f, bool comma = true) {
    printf("\"%s\":{\"v\":%.10g,\"src\":\"%s\"}%s", k, f.v, srcName(f.s), comma ? "," : "");
}
// Profile JSON for ears: facts with status, plus the designed filter response (anti-alias x reconstruction) at sr.
static void dumpProfile(const BbdProfile& p, int sr) {
    printf("{\"id\":\"%s\",", p.id);
    dumpFact("stages", p.stages); dumpFact("delay_min_ms", p.delayMinMs); dumpFact("delay_max_ms", p.delayMaxMs);
    dumpFact("chip_clk_min_hz", p.chipClkMinHz); dumpFact("chip_clk_max_hz", p.chipClkMaxHz); dumpFact("snr_db", p.snrDb);
    dumpFact("comp_tau_ms", p.compTauMs); dumpFact("nl_a", p.nlA); dumpFact("nl_b", p.nlB);
    printf("\"clk_at_min_delay_hz\":%.6g,\"clk_at_max_delay_hz\":%.6g,\"overclock\":%s,\"underclock\":%s,",
           clockHz(p, p.delayMinMs.v), clockHz(p, p.delayMaxMs.v), p.overclock ? "true" : "false", p.underclock ? "true" : "false");
    // Feedback scale the engine applies at this rate: fb x scale = peak loop gain; compander on / bypassed.
    printf("\"loop_norm\":{\"comp\":%.10g,\"bypass\":%.10g},", loopNorm(p, sr, true), loopNorm(p, sr, false));
    printf("\"compander\":%s,\"has_dark\":%s,\"filter_src\":\"%s\",\"fb_src\":\"%s\",\"sets\":[",
           p.compander ? "true" : "false", p.hasDark ? "true" : "false", srcName(p.filterSrc), srcName(p.fbSrc));
    static const double fr[] = {100, 200, 500, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 8000};
    for (int s = 0; s < (p.hasDark ? 2 : 1); ++s) {
        SosCascade a, r; a.design(p.aa[s], sr); r.design(p.recon[s], sr);
        auto db = [&](double f) { return a.magDb(f, sr) + r.magDb(f, sr) - (a.magDb(200, sr) + r.magDb(200, sr)); };
        double f3 = -1, w3 = -1, pk = -1e9;
        for (double f = 201; f < 0.45 * sr; f += 1) {
            const double v = db(f); pk = std::max(pk, v);
            if (f3 < 0 && v < -3.0) f3 = f;
            if (w3 < 0 && 2.0 * v < -3.0) w3 = f;
        }
        printf("%s{\"name\":\"%s\",", s ? "," : "", s ? "dark" : "bright");
        dumpFact("wet_bw_hz", p.wetBwHz[s]);
        // Impulse-response peak of the chain (ms): where an echo's peak lands after the BBD delay itself.
        SosCascade ia = a, ir = r; ia.reset(); ir.reset(); double irPk = 0, irAt = 0;
        for (int n = 0; n < sr / 10; ++n) { const double v = std::fabs(ir.tick(ia.tick(n == 0 ? 1.0 : 0.0))); if (v > irPk) { irPk = v; irAt = n; } }
        printf("\"filter_f3_hz\":%g,\"wet_f3_hz\":%g,\"peak_db\":%.3f,\"gain_200_db\":%.4f,\"ir_peak_ms\":%.4f,\"resp_db\":{",
               f3, w3, pk, a.magDb(200, sr) + r.magDb(200, sr), irAt * 1000.0 / sr);
        for (size_t i = 0; i < sizeof fr / sizeof fr[0]; ++i) printf("%s\"%g\":%.4f", i ? "," : "", fr[i], db(fr[i]));
        printf("}}");
    }
    printf("]}");
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> a;
    for (int i = 1; i + 1 < argc; i += 2) a[argv[i]] = argv[i + 1];
    auto num = [&](const char* k, double d) { return a.count(k) ? atof(a[k].c_str()) : d; };
    if (a.count("--dump-profile")) {
        const int dsr = (int) num("--sr", 48000); const std::string id = a["--dump-profile"];
        if (id == "all") { printf("["); for (int i = 0; i < profileCount(); ++i) { if (i) printf(","); dumpProfile(profileAt(i), dsr); } printf("]\n"); return 0; }
        BbdProfile p; if (!findProfile(id.c_str(), p)) { fprintf(stderr, "bad --dump-profile\n"); return 2; }
        dumpProfile(p, dsr); printf("\n"); return 0;
    }
    if (!a.count("--out")) { fprintf(stderr, "need --out\n"); return 2; }
    int sr = (int) num("--sr", 48000);
    std::vector<float> in;
    if (a.count("--in")) { if (!readWav(a["--in"].c_str(), in, sr)) { fprintf(stderr, "bad --in\n"); return 2; } }
    else {
        in.assign((size_t) (num("--secs", 4) * sr), 0.0f);
        if (a["--stim"] == "tone") {  // sine, 10 ms fade-in/out to avoid a start click
            const double f = num("--freq", 1000), lv = num("--level", 0.5); const size_t fade = (size_t) (0.01 * sr);
            for (size_t i = 0; i < in.size(); ++i) {
                double g = i < fade ? (double) i / fade : (i + fade > in.size() ? (double) (in.size() - i) / fade : 1.0);
                in[i] = (float) (lv * g * std::sin(2.0 * M_PI * f * (double) i / sr)); }
        } else if (a["--stim"] == "twotone") {  // two sines (--freq/--level, --freq2/--level2), same 10 ms fades as tone
            const double f = num("--freq", 1000), lv = num("--level", 0.25), f2 = num("--freq2", 3450), lv2 = num("--level2", 0.25);
            const size_t fade = (size_t) (0.01 * sr);
            for (size_t i = 0; i < in.size(); ++i) {
                double g = i < fade ? (double) i / fade : (i + fade > in.size() ? (double) (in.size() - i) / fade : 1.0);
                const double t = (double) i / sr;
                in[i] = (float) (g * (lv * std::sin(2.0 * M_PI * f * t) + lv2 * std::sin(2.0 * M_PI * f2 * t))); }
        } else if (a["--stim"] == "noise") {  // deterministic LCG white noise
            uint32_t st = 12345u; const double lv = num("--level", 0.25);
            for (auto& v : in) { st = st * 1664525u + 1013904223u; v = (float) (lv * ((st >> 8) / 8388608.0 - 1.0)); }
        } else if (a["--stim"] == "burst") {  // 1 kHz bursts, 20 ms, 2 ms raised-cosine fades, one per beat
            const double f = num("--freq", 1000), lv = num("--level", 0.5), bpm = num("--bpm", 120.0);
            const size_t first = (size_t) (0.1 * sr), len = (size_t) (0.02 * sr + 0.5), fade = (size_t) (0.002 * sr + 0.5);
            const size_t beat = (size_t) (60.0 / bpm * sr + 0.5);
            for (size_t t = first; t < in.size(); t += beat)
                for (size_t n = 0; n < len && t + n < in.size(); ++n) {
                    double g = 1.0;
                    if (n < fade) g = 0.5 - 0.5 * std::cos(M_PI * (double) n / fade);
                    else if (n + fade >= len) g = 0.5 - 0.5 * std::cos(M_PI * (double) (len - 1 - n) / fade);
                    in[t + n] = (float) (lv * g * std::sin(2.0 * M_PI * f * (double) n / sr)); }
        } else {
        const bool clicks = a["--stim"] == "clicks";
        const size_t first = (size_t) (0.1 * sr);
        const double bpm = num("--bpm", 120.0);
        const size_t beat = (size_t) (60.0 / bpm * sr + 0.5);
        for (size_t t = first; t < in.size(); t += clicks ? beat : in.size()) in[t] = 1.0f;
        }
    }
    if (a.count("--write-in") && !writeWav(a["--write-in"].c_str(), in, sr)) { fprintf(stderr, "bad --write-in\n"); return 2; }
    const std::string engine = a.count("--engine") ? a["--engine"] : "naive";
    if (engine != "naive" && engine != "tape" && engine != "bbd") { fprintf(stderr, "bad --engine\n"); return 2; }
    const bool tape = engine == "tape", bbd = engine == "bbd";
    BbdProfile prof{};
    if (bbd && !findProfile(a.count("--profile") ? a["--profile"].c_str() : "m8192", prof)) { fprintf(stderr, "bad --profile\n"); return 2; }
    int probe = -1;
    if (a.count("--probe")) {
        const std::string& pr = a["--probe"];
        if (bbd) probe = pr == "clk" ? 0 : pr == "bbd_ms" ? 1 : -1;
        else probe = pr == "v" ? 0 : pr == "tau1" ? 1 : pr == "tau2" ? 2 : pr == "tau3" ? 3
              : pr == "tauerr1" ? 4 : pr == "tauerr2" ? 5 : pr == "tauerr3" ? 6 : pr == "clamped" ? 7 : -1;
        if (probe < 0) { fprintf(stderr, "bad --probe\n"); return 2; }
    }
    Live L; L.bpm = num("--bpm", 120.0); L.div = num("--div", 0.5); L.fb = num("--fb", 0.5); L.delayMsGiven = a.count("--delay-ms") > 0;
    L.tp.bpm = (float) L.bpm; L.tp.division = (float) L.div;
    if (L.delayMsGiven) { L.tp.sync = 0; L.tp.time_ms = (float) num("--delay-ms", 0); }
    if (a.count("--params") && !applyList(L, a["--params"])) return 2;
    std::vector<SchedEv> sched;
    if (a.count("--sched")) {
        for (const auto& ev : split(a["--sched"], ';')) {
            if (ev.empty()) continue;
            const size_t c = ev.find(':'); char* end = nullptr;
            const double t = c == std::string::npos ? -1.0 : strtod(ev.substr(0, c).c_str(), &end);
            if (c == std::string::npos || t < 0 || *end) { fprintf(stderr, "bad --sched event '%s'\n", ev.c_str()); return 2; }
            Live scratch = L;  // validate keys now so errors exit before rendering
            if (!applyList(scratch, ev.substr(c + 1))) return 2;
            sched.push_back({(size_t) std::llround(t * sr), ev.substr(c + 1)});
        }
        std::stable_sort(sched.begin(), sched.end(), [](const SchedEv& x, const SchedEv& y) { return x.at < y.at; });
    }
    NaiveEcho e; TapeEngine te; BbdEngine be;
    const double dry = num("--dry", 1), wet = num("--wet", 1);
    auto naiveSync = [&]() {
        const double m = L.delayMsGiven ? num("--delay-ms", 0) : L.div * 60000.0 / L.bpm;
        e.delaySamples = (float) (m * sr / 1000.0); e.feedback = (float) L.fb; return m;
    };
    double ms;
    L.bp.time_ms = L.tp.time_ms; L.bp.feedback = (float) L.fb;
    if (bbd) { be.prepare(sr, prof); be.setParams(L.bp); ms = L.bp.time_ms; }
    else if (tape) { te.prepare(sr, 8.0); te.setParams(L.tp); ms = L.delayMsGiven ? num("--delay-ms", 0) : L.div * 60000.0 / L.bpm; }
    else { e.prepare(sr); ms = naiveSync(); e.dry = (float) dry; e.wet = (float) wet; }
    std::vector<float> out(in.size());
    size_t si = 0;
#ifdef TIDBANK_DEBUG_PROBE
    probe::Hub hub; probe::Tracer tracer; FILE* pcsv = nullptr; const size_t pblk = (size_t) std::max(1.0, num("--probe-block", 512));
    char pline[512]; bool pInBlock = false;
    if (a.count("--probe-csv")) {
        pcsv = fopen(a["--probe-csv"].c_str(), "w"); if (!pcsv) { fprintf(stderr, "bad --probe-csv\n"); return 2; }
        probe::Hub::Config pc; pc.intervalSec = num("--probe-interval", 1.0); pc.firstSec = a.count("--probe-offset-s") ? num("--probe-offset-s", 0.0) : -1.0;
        if (pc.intervalSec <= 0.0) { fprintf(stderr, "bad --probe-interval\n"); return 2; }
        hub.prepare(sr, pc); hub.bpm = L.bpm;
        if (bbd) { tracer.init(&hub, 1, prof.id); be.attachProbe(&tracer); } else if (tape) { tracer.init(&hub, 0, "tape"); te.attachProbe(&tracer); }
        fprintf(pcsv, "%s\n", probe::csvHeader());
    }
    auto drain = [&]() { probe::Rec r; while (hub.pop(r)) { probe::formatCsv(r, sr, pline, sizeof pline); fprintf(pcsv, "%s\n", pline); } };
#else
    if (a.count("--probe-csv")) { fprintf(stderr, "--probe-csv needs a build with TIDBANK_DEBUG_PROBE (tidbank_render_probe)\n"); return 2; }
#endif
    for (size_t i = 0; i < in.size(); ++i) {
#ifdef TIDBANK_DEBUG_PROBE
        if (pcsv && i % pblk == 0) { if (pInBlock) { hub.blockEnd(); drain(); } hub.blockBegin((int) std::min(pblk, in.size() - i)); pInBlock = true; }
#endif
        if (si < sched.size() && sched[si].at <= i) {
            while (si < sched.size() && sched[si].at <= i) applyList(L, sched[si++].list);
#ifdef TIDBANK_DEBUG_PROBE
            hub.bpm = L.bpm;
#endif
            if (bbd) be.setParams(L.bp); else if (tape) te.setParams(L.tp); else naiveSync();
        }
        if (bbd) {
            const float w = be.process(in[i]);
            out[i] = probe >= 0 ? (float) be.probe(probe) : (float) (in[i] * dry + w * wet);
        } else if (tape) {
            const float w = te.process(in[i]);
            out[i] = probe >= 0 ? (float) te.probe(probe) : (float) (in[i] * dry + w * wet);
        } else out[i] = probe >= 0 ? 0.0f : e.process(in[i]);
#ifdef TIDBANK_DEBUG_PROBE
        if (pcsv) hub.advance();
#endif
    }
#ifdef TIDBANK_DEBUG_PROBE
    if (pcsv) { if (pInBlock) hub.blockEnd(); drain(); fclose(pcsv); if (hub.drops()) fprintf(stderr, "probe: %llu records dropped\n", (unsigned long long) hub.drops()); }
#endif
    if (!writeWav(a["--out"].c_str(), out, sr)) return 1;
    printf("{\"sr\":%d,\"samples\":%zu,\"delay_ms\":%.4f}\n", sr, out.size(), ms);
    return 0;
}
