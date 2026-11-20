#!/usr/bin/env python3
"""TidBank ears, step 2: measure a rendered WAV. Numbers, never plots, are the agent's evidence
(an earlier benchmark: VLMs read spectrograms at chance level). Prints JSON; exit 1 on a failed check.
Run: uv run --with numpy tools/ears/analyze.py <cmd> file.wav [opts]   (first channel of the WAV is analysed)

  taps        echo times + levels; --expect-ms N checks tap k at k*N within --tol-ms
  spectrum    per-tap spectral SHAPE vs tap 1, in dB at --freqs, normalised to 1 kHz (isolates filtering from level)
  wowflutter  tone in -> % deviation in wow (0.5-6 Hz) and flutter (6-200 Hz) bands, unweighted, + dominant mod freq
  glide       tone in -> frequency ratio over time (pitch glide), peak/final ratio, settle time, area_s = sum(ratio-1)dt
              (for an echo of a steady tone area_s == old delay - new delay in seconds, whatever the dynamics)
  thd         sine in -> THD (dB), H2..H5 and inharmonic_db (everything outside the harmonic bins and DC, re fundamental)
  alias       two-tone in (pilot --f0 + probe --fp) through a pitch glide: power of everything that is not a harmonic of the
              pilot nor the shifted probe, re probe input power (dB); STFT 8192/2048, Blackman-Harris
  bursts      tone bursts: RMS (dB) of the central 16 ms of each burst/echo at onset + k*--period-ms, and step_db between them
  health      NaN/Inf, subnormals, DC, peak
Self-test of every analyzer on synthetic signals: tools/ears/selftest.py"""
import sys, json, struct, argparse
import numpy as np

# ---------------------------------------------------------------- io
def read_wav(path):
    b = open(path, 'rb').read(); i = 12; fmt = None
    while i < len(b):
        cid, sz = b[i:i+4], struct.unpack('<I', b[i+4:i+8])[0]; d = b[i+8:i+8+sz]
        if cid == b'fmt ': tag, ch, sr, _, _, bits = struct.unpack('<HHIIHH', d[:16]); fmt = (tag, ch, sr, bits)
        elif cid == b'data':
            tag, ch, sr, bits = fmt
            if tag == 3: x = np.frombuffer(d, '<f4')
            elif bits == 16: x = np.frombuffer(d, '<i2') / 32768.0
            else:
                r = np.frombuffer(d, np.uint8).reshape(-1, 3).astype(np.int32)
                x = ((r[:, 0] << 8 | r[:, 1] << 16 | r[:, 2] << 24) >> 8) / 8388608.0
            return x.reshape(-1, ch)[:, 0].astype(np.float64), sr
        i += 8 + sz + (sz & 1)
    raise ValueError('no data chunk')

# ---------------------------------------------------------------- taps
def taps(x, sr, floor_db=-70.0, min_gap_ms=5.0):
    """Echo taps = local maxima of |x| above floor relative to the loudest, >= min_gap apart. Returns [(pos_samples, amp)]."""
    a = np.abs(x); thr = a.max() * 10 ** (floor_db / 20); gap = int(min_gap_ms * sr / 1000); out = []
    idx = np.flatnonzero((a[1:-1] >= a[:-2]) & (a[1:-1] > a[2:]) & (a[1:-1] > thr)) + 1
    for k in idx:
        if out and k - out[-1] < gap:
            if a[k] > a[out[-1]]: out[-1] = k
        else: out.append(k)
    res = []
    for k in out:  # parabolic interpolation: sub-sample position
        y0, y1, y2 = a[k-1], a[k], a[k+1]; den = y0 - 2*y1 + y2
        off = 0.5 * (y0 - y2) / den if den else 0.0
        res.append((k + off, y1))
    return res

# ---------------------------------------------------------------- spectrum per tap
def band_db(seg, sr, f, nfft=1 << 15):
    """Mean power (dB) of seg's spectrum in a 1/3-octave band around f."""
    w = np.hanning(len(seg)); X = np.abs(np.fft.rfft(seg * w, nfft)) ** 2
    fr = np.fft.rfftfreq(nfft, 1 / sr); m = (fr >= f / 2 ** (1/6)) & (fr <= f * 2 ** (1/6))
    return 10 * np.log10(X[m].mean() + 1e-30)

def tap_shapes(x, sr, tap_pos, freqs, win_ms=20.0):
    """For each tap: band dB at freqs, minus its band dB at 1 kHz, minus the same for tap 1. Tap 1 is all zeros by definition.
    The Hann window is CENTRED on the tap: a tap on the window skirt is weighted unevenly (this bit us in the self-test).
    win_ms must stay below the tap spacing."""
    rows = []
    for p in tap_pos:
        s = max(0, int(p - win_ms * sr / 2000)); seg = x[s:s + int(win_ms * sr / 1000)]
        rows.append([band_db(seg, sr, f) for f in [1000.0] + list(freqs)])
    rows = np.array(rows); shape = rows[:, 1:] - rows[:, :1]
    return shape - shape[0]

# ---------------------------------------------------------------- instantaneous frequency
def inst_freq(x, sr):
    """Instantaneous frequency (Hz) of a tone via FFT-based analytic signal. len = len(x)-1."""
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1
    if n % 2 == 0: h[n // 2] = 1; h[1:n // 2] = 2
    else: h[1:(n + 1) // 2] = 2
    ph = np.unwrap(np.angle(np.fft.ifft(X * h)))
    return np.diff(ph) * sr / (2 * np.pi)

def wow_flutter(x, sr, trim_s=0.5):
    f = inst_freq(x, sr); t = int(trim_s * sr); f = f[t:-t]; f0 = f.mean(); d = (f - f0) / f0
    D = np.fft.rfft(d); fr = np.fft.rfftfreq(len(d), 1 / sr); out = {'carrier_hz': round(float(f0), 3)}
    for name, lo, hi in (('wow', 0.5, 6.0), ('flutter', 6.0, 200.0)):
        y = np.fft.irfft(D * ((fr >= lo) & (fr < hi)), n=len(d))
        out[name + '_rms_pct'] = round(float(y.std() * 100), 5); out[name + '_pp_pct'] = round(float((y.max() - y.min()) * 100), 5)
    m = (fr >= 0.3) & (fr <= 200)
    out['dominant_mod_hz'] = round(float(fr[m][np.argmax(np.abs(D[m]))]), 3)
    return out

def glide(x, sr, hop_ms=10.0, tol=0.001, ref_s=0.5, f0=None):
    # f0: input tone frequency (reference); default = median of the first ref_s seconds of x
    f = inst_freq(x, sr); f0 = float(f0) if f0 else float(np.median(f[:int(ref_s * sr)])); hop = int(hop_ms * sr / 1000)
    m = len(f) // hop; r = np.median((f[:m * hop] / f0).reshape(m, hop), axis=1); t = (np.arange(m) + 0.5) * hop / sr
    final = float(np.median(r[-max(1, m // 10):])); off = np.flatnonzero(np.abs(r - final) > tol)
    settle = float(t[off[-1]] + hop_ms / 1000) if len(off) else 0.0
    step = max(1, int(100 / hop_ms))
    return {'ref_hz': round(f0, 3), 'min_ratio': round(float(r.min()), 5), 'max_ratio': round(float(r.max()), 5),
            'final_ratio': round(final, 5), 'settle_s': round(settle, 3),
            'area_s': round(float(np.sum(r - 1.0) * hop_ms / 1000), 5),  # = tau_old - tau_new for an echo repeat of a steady tone
            'timeline_100ms': [round(float(v), 5) for v in r[::step][:80]]}

# ---------------------------------------------------------------- step (probe signals, e.g. --probe v)
def step(x, sr, t_step):
    # Step response of a probe signal: start/final value, overshoot (relative to the step), 99 % settle time.
    i = int(round(t_step * sr)); v0 = float(x[max(i - 1, 0)]); v1 = float(x[-1]); dv = v1 - v0
    seg = x[i:].astype(np.float64)
    over = float(((seg - v1) / dv).max()) if dv else 0.0
    out = np.flatnonzero(np.abs(seg - v1) > 0.01 * abs(dv))
    settle = float((out[-1] + 1) / sr) if len(out) else 0.0
    return {'v0': v0, 'v1': v1, 'overshoot_rel': over, 'settle99_ms': round(settle * 1000, 3)}
# ---------------------------------------------------------------- thd
def bh_window(n):
    k = np.arange(n); c = 2 * np.pi * k / (n - 1)
    return 0.35875 - 0.48829 * np.cos(c) + 0.14128 * np.cos(2 * c) - 0.01168 * np.cos(3 * c)

def bh7_window(n):
    a = [0.27105140069342, 0.43329793923448, 0.21812299954311, 0.06592544638803, 0.01081174209837, 0.00077658482522, 0.00001388721735]
    c = 2 * np.pi * np.arange(n) / (n - 1); return sum(((-1) ** k) * a[k] * np.cos(k * c) for k in range(7))

def thd(x, sr, f0=None, nharm=10, bins=8):
    # 7-term Blackman-Harris (main lobe +-7 bins): the 4-term window's leakage floor (~-90 dB off-bin) hid inharmonic content
    n = len(x); X = np.abs(np.fft.rfft(x * bh7_window(n))) ** 2; fr = np.fft.rfftfreq(n, 1 / sr)
    if f0 is None: f0 = fr[np.argmax(X[1:]) + 1]
    def pw(f):
        c = int(round(f * n / sr)); return X[max(0, c - bins):c + bins + 1].sum()
    p1 = pw(f0); hs = {k: pw(k * f0) for k in range(2, nharm + 1) if k * f0 < sr / 2 - 100}
    tot = np.sqrt(sum(hs.values()) / p1)
    # inharmonic: all power outside +-bins of k*f0 (k = 1..max below Nyquist) and outside DC (< 20 Hz), re fundamental
    keep = fr >= 20.0
    for k in range(1, int((sr / 2) // f0) + 1):
        c = int(round(k * f0 * n / sr)); keep[max(0, c - bins):c + bins + 1] = False
    inh = X[keep].sum() / p1
    return {'f0_hz': round(float(f0), 2), 'thd_db': round(float(20 * np.log10(tot + 1e-30)), 2),
            'inharmonic_db': round(float(10 * np.log10(inh + 1e-30)), 2),
            **{f'h{k}_db': round(float(10 * np.log10(hs[k] / p1 + 1e-30)), 2) for k in range(2, 6) if k in hs}}

# ---------------------------------------------------------------- alias (two-tone through a glide)
def alias(x, sr, f1=1000.0, fp=3450.0, probe_amp=0.25, all_frames=False, nfft=8192, hop=2048, nharm=8, floor_hz=200.0, min_bins=8):
    """7-term Blackman-Harris (-180 dB sidelobes), exclusion min +-8 bins (its main lobe is +-7): the 4-term BH leaves a
    leakage floor of only ~-88..-99 dB re an equal-amplitude pilot, which would drown the -100 dB target.
    Pilot f1 + probe fp in, speed ratio r = pilot inst freq / f1 per frame. Alias power per frame = power in [floor_hz, Nyquist]
    outside +-3 % (min +-min_bins bins) of r*k*f1 (k=1..nharm) and of r*fp, re probe input power probe_amp^2/2, dB.
    Frames count when |r-1| > 0.001 (or all, static case); frames whose pilot power is < -40 dB of the max are skipped.
    fp must lie outside 0.5..1.6 x f1 (the pilot band-pass), else r is polluted by the probe."""
    n = len(x); X = np.fft.rfft(x); fr = np.fft.rfftfreq(n, 1 / sr)
    pil = np.fft.irfft(X * ((fr >= 0.5 * f1) & (fr <= 1.6 * f1)), n=n)   # pilot region only
    h = np.zeros(n); h[0] = 1
    if n % 2 == 0: h[n // 2] = 1; h[1:n // 2] = 2
    else: h[1:(n + 1) // 2] = 2
    ph = np.unwrap(np.angle(np.fft.ifft(np.fft.fft(pil) * h)))
    # per-sample ratio (smoothed over 5 ms): a glide sweeps the probe within one frame, so the exclusion bands
    # must cover the frame's whole ratio range, not just its mean (else the chirp itself reads as alias)
    rl = np.diff(ph) * sr / (2 * np.pi) / f1; m5 = max(1, int(0.005 * sr)); rl = np.convolve(rl, np.ones(m5) / m5, mode='same')
    win = bh7_window(nfft); wn = float(np.sum(win ** 2)); fb = np.fft.rfftfreq(nfft, 1 / sr); binhz = sr / nfft
    pp = np.array([np.mean(pil[s:s + nfft] ** 2) for s in range(0, n - nfft + 1, hop)])
    ref = 0.5 * probe_amp ** 2; out = []; rs = []
    for fi, s in enumerate(range(0, n - nfft + 1, hop)):
        if pp[fi] < pp.max() * 1e-4: continue          # silent frame (pilot < -40 dB of max)
        r = (ph[s + nfft - 1] - ph[s]) / (nfft - 1) * sr / (2 * np.pi) / f1
        if not all_frames and abs(r - 1.0) <= 0.001: continue
        P = np.abs(np.fft.rfft(x[s:s + nfft] * win)) ** 2; keep = fb >= floor_hz
        seg = rl[s + m5:s + nfft - m5]; lo, hi = float(seg.min()), float(seg.max())
        def excl(c):  # c = frequency at ratio 1; band spans the frame's ratio range, +-3 % (min +-min_bins bins)
            a, b = lo * c, hi * c; pad = max(0.03 * hi * c, min_bins * binhz)
            return (fb < a - pad) | (fb > b + pad)
        for k in range(1, nharm + 1):
            if lo * k * f1 < sr / 2: keep &= excl(k * f1)
        if lo * fp < sr / 2: keep &= excl(fp)
        out.append(10 * np.log10(2 * P[keep].sum() / (nfft * wn) / ref + 1e-30)); rs.append(r)
    if not out: return {'frames_used': 0, 'ok': False, 'error': 'no frames used'}
    res = {'alias_db_max': round(float(max(out)), 2), 'alias_db_median': round(float(np.median(out)), 2), 'frames_used': len(out),
           'ratio_min': round(float(min(rs)), 5), 'ratio_max': round(float(max(rs)), 5)}
    if 0.5 * f1 <= fp <= 1.6 * f1: res['warning'] = 'fp inside pilot band-pass 0.5..1.6 x f0: ratio unreliable'
    return res

# ---------------------------------------------------------------- bursts (echo train of tone bursts)
def bursts(x, sr, onset_s, period_ms, max_taps, skip_first=False, win_ms=16.0):
    """RMS (dB re 1.0) over the central win_ms of a 20 ms burst/echo starting at onset_s + k*period_ms, k = 0..max_taps
    (k=0 is the dry burst; skip_first starts at k=1). step_db = successive differences. Windows beyond the end are dropped."""
    L = int(round(win_ms * sr / 1000)); off = int(round(10 * sr / 1000)) - L // 2; db = []; ks = []
    for k in range(1 if skip_first else 0, max_taps + 1):
        s = int(round((onset_s + k * period_ms / 1000) * sr)) + off
        if s < 0 or s + L > len(x): break
        db.append(10 * np.log10(np.mean(x[s:s + L] ** 2) + 1e-30)); ks.append(k)
    return {'k': ks, 'db': [round(float(v), 3) for v in db], 'step_db': [round(float(b - a), 3) for a, b in zip(db, db[1:])]}

# ---------------------------------------------------------------- click (discontinuity at a known time)
def click(x, sr, t_switch, hp_hz=2500.0, win_ms=50.0, edge_s=0.2):
    # Brick-wall high-pass (FFT mask) well above the test tone, then peak within +-win_ms of the switch, dBFS.
    # baseline_db = same peak in a window edge_s earlier (no switch): the test is meaningful only if it is far lower.
    n = len(x); X = np.fft.rfft(x.astype(np.float64)); X[np.fft.rfftfreq(n, 1 / sr) < hp_hz] = 0; y = np.fft.irfft(X, n=n)
    w = int(win_ms * sr / 1000)
    def pk(t): i = int(t * sr); return float(20 * np.log10(np.abs(y[max(0, i - w):i + w]).max() + 1e-30))
    return {'click_db': round(pk(t_switch), 2), 'baseline_db': round(pk(t_switch - edge_s), 2)}

# ---------------------------------------------------------------- stats (probe signals in a window)
def stats(x):
    x = x.astype(np.float64)
    return {'min': float(x.min()), 'max': float(x.max()), 'max_abs': float(np.abs(x).max()), 'mean': float(x.mean())}

# ---------------------------------------------------------------- health
def health(x):
    a = np.abs(x); fin = np.isfinite(x); sub = int(np.count_nonzero((a > 0) & (a < 1.1754944e-38)))
    return {'finite': bool(fin.all()), 'subnormals': sub, 'peak': float(a[fin].max()) if fin.any() else None,
            'peak_dbfs': round(float(20 * np.log10(a[fin].max() + 1e-30)), 2) if fin.any() else None,
            'dc': float(x[fin].mean()) if fin.any() else None, 'over_0dbfs': bool((a[fin] > 1.0).any()),
            'ok': bool(fin.all()) and sub == 0}

# ---------------------------------------------------------------- cli
def tf(x, ref, sr, fmax=None, nseg=1 << 14):
    """|H| = |Sxy / Sxx| (Welch, Hann, 50 % overlap) after aligning y to x by the cross-correlation peak (BBD delay).
    Returns delay (ms), dB re the 200 Hz bin on a frequency grid, the first -3 dB crossing above the passband peak,
    and coherence. LTI paths only (compander off)."""
    n = min(len(x), len(ref)); x, ref = x[:n], ref[:n]
    N = 1 << int(np.ceil(np.log2(2 * n)))
    xc = np.fft.irfft(np.fft.rfft(x, N) * np.conj(np.fft.rfft(ref, N)), N)
    lag = int(np.argmax(np.abs(xc[:n])))
    y, u = x[lag:], ref[:n - lag]
    w = np.hanning(nseg); hop = nseg // 2
    Sxy = 0; Sxx = 0; Syy = 0
    for i in range(0, len(u) - nseg + 1, hop):
        U = np.fft.rfft(w * u[i:i + nseg]); Y = np.fft.rfft(w * y[i:i + nseg])
        Sxy = Sxy + Y * np.conj(U); Sxx = Sxx + np.abs(U) ** 2; Syy = Syy + np.abs(Y) ** 2
    f = np.fft.rfftfreq(nseg, 1 / sr)
    H = np.abs(Sxy) / np.maximum(Sxx, 1e-300); coh = np.abs(Sxy) ** 2 / np.maximum(Sxx * Syy, 1e-300)
    db = 20 * np.log10(np.maximum(H, 1e-12)); db -= db[np.argmin(np.abs(f - 200))]
    sel = (f > 200) & (f < (fmax or 0.45 * sr))
    ipk = np.argmax(np.where(sel, db, -1e9)); f3 = None
    for i in range(ipk, len(f)):
        if sel[i] and db[i] < -3.0: f3 = float(f[i - 1] + (f[i] - f[i - 1]) * (db[i - 1] + 3.0) / (db[i - 1] - db[i])); break
    grid = [100, 200, 500, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 8000]
    return {'delay_ms': round(lag * 1000 / sr, 4), 'f3_hz': f3, 'peak_db': round(float(db[ipk]), 3),
            'resp_db': {str(g): round(float(db[np.argmin(np.abs(f - g))]), 3) for g in grid},
            'coh_min_passband': round(float(coh[(f > 100) & (f < (f3 or 1000))].min()), 4)}

def repeat(x, sr, onset_s, period_ms, freqs, k=1):
    """Per-repeat response of an LTI echo loop: |FFT(tap k+1)| / |FFT(tap k)| at exact frequencies (dB).
    Both windows start 2 ms before the nominal tap and span 0.9 of the period (Hann on the last 20 %), so any fixed
    delay offset is the same in both and drops out of the magnitude."""
    P = period_ms / 1000; L = int(0.9 * P * sr); nf = 1 << int(np.ceil(np.log2(8 * L)))
    w = np.ones(L); t = int(0.2 * L); w[L - t:] = np.hanning(2 * t)[t:]
    def spec(j):
        i0 = int((onset_s + j * P - 0.002) * sr); seg = x[i0:i0 + L]
        return np.fft.rfft(w[:len(seg)] * seg, nf)
    S1, S2 = spec(k), spec(k + 1); f = np.fft.rfftfreq(nf, 1 / sr)
    out = {}
    for g in freqs:
        i = int(np.argmin(np.abs(f - g))); out[str(int(g))] = round(float(20 * np.log10(np.abs(S2[i]) / max(np.abs(S1[i]), 1e-300))), 3)
    return {'ratio_db': out}

def level(x, sr, start_s, end_s):
    seg = x[int(start_s * sr):int(end_s * sr) if end_s else len(x)]
    return {'rms_db': round(float(10 * np.log10(np.mean(seg ** 2) + 1e-300)), 4), 'peak_db': round(float(20 * np.log10(np.abs(seg).max() + 1e-300)), 3)}

def main(argv=None):
    p = argparse.ArgumentParser(); p.add_argument('cmd', choices=['taps', 'spectrum', 'wowflutter', 'glide', 'thd', 'health', 'step', 'stats', 'click', 'alias', 'bursts', 'tf', 'level', 'repeat'])
    p.add_argument('--ref')  # tf: the stimulus WAV (tidbank_render --write-in)
    p.add_argument('wav'); p.add_argument('--onset-s', type=float, default=0.1)
    p.add_argument('--expect-ms', type=float); p.add_argument('--tol-ms', type=float, default=0.1); p.add_argument('--max-taps', type=int, default=8)
    p.add_argument('--windows'); p.add_argument('--hp-hz', type=float, default=2500.0); p.add_argument('--max-click-db', type=float); p.add_argument('--floor-db', type=float, default=-70.0); p.add_argument('--min-gap-ms', type=float, default=5.0)  # taps: ignore ringing
    p.add_argument('--freqs', default='100,250,1000,4000,10000'); p.add_argument('--max-thd-db', type=float)
    p.add_argument('--win-ms', type=float, default=20.0)  # spectrum: analysis window per tap, must stay below tap spacing
    p.add_argument('--max-inharmonic-db', type=float); p.add_argument('--fp', type=float); p.add_argument('--probe-amp', type=float, default=0.25)
    p.add_argument('--max-alias-db', type=float); p.add_argument('--all-frames', action='store_true')
    p.add_argument('--period-ms', type=float); p.add_argument('--skip-first', action='store_true')
    p.add_argument('--f0', type=float); p.add_argument('--trim-s', type=float, default=0.5)
    p.add_argument('--t-step', type=float, default=0.0); p.add_argument('--start-s', type=float, default=0.0); p.add_argument('--end-s', type=float)  # glide window
    a = p.parse_args(argv); x, sr = read_wav(a.wav)
    if a.cmd == 'health': r = health(x); print(json.dumps(r)); return 0 if r['ok'] else 1
    if not np.all(np.isfinite(x)): print(json.dumps({'ok': False, 'error': 'NaN/Inf in output'})); return 1
    res = {'sr': sr, 'ok': True}
    if a.cmd in ('taps', 'spectrum'):
        t = [(pos, v) for pos, v in taps(x, sr, a.floor_db, a.min_gap_ms) if (pos - a.onset_s * sr) * 1000 / sr > 1.0][:a.max_taps]
        rows = [{'t_ms': round((pos - a.onset_s * sr) * 1000 / sr, 4), 'db': round(20 * np.log10(v), 2)} for pos, v in t]
        if a.cmd == 'taps':
            res['peak'] = float(np.abs(x).max()); res['taps'] = rows
            if a.expect_ms is not None:
                errs = [round(e['t_ms'] - a.expect_ms * (i + 1), 4) for i, e in enumerate(rows)]
                res['expect_ms'] = a.expect_ms; res['err_ms'] = errs
                res['ok'] = bool(rows) and all(abs(e) <= a.tol_ms for e in errs)
        else:
            fl = [float(f) for f in a.freqs.split(',')]; sh = tap_shapes(x, sr, [pos for pos, _ in t], fl, win_ms=a.win_ms)
            res['freqs_hz'] = fl; res['shape_db_vs_tap1'] = [[round(float(v), 2) for v in row] for row in sh]; res['taps_ms'] = [r['t_ms'] for r in rows]
    elif a.cmd == 'wowflutter': res.update(wow_flutter(x, sr, a.trim_s))
    elif a.cmd == 'glide': res.update(glide(x[int(a.start_s * sr):int(a.end_s * sr) if a.end_s else len(x)], sr, f0=a.f0))
    elif a.cmd == 'click':
        res.update(click(x, sr, a.t_step, a.hp_hz))
        if a.max_click_db is not None: res['ok'] = res['click_db'] <= a.max_click_db
    elif a.cmd == 'stats':
        res.update(stats(x[int(a.start_s * sr):int(a.end_s * sr) if a.end_s else len(x)]))
        if a.windows:  # "10:30,30:60" -> peak dBFS per window (bounded-loop checks)
            ws = [tuple(float(v) for v in w.split(':')) for w in a.windows.split(',')]
            res['window_peak_db'] = [round(float(20 * np.log10(np.abs(x[int(s0 * sr):int(s1 * sr)]).max() + 1e-30)), 3) for s0, s1 in ws]
    elif a.cmd == 'step': res.update(step(x, sr, a.t_step))
    elif a.cmd == 'tf':
        if not a.ref: p.error('tf needs --ref')
        res.update(tf(x, read_wav(a.ref)[0], sr))
    elif a.cmd == 'level': res.update(level(x, sr, a.start_s, a.end_s))
    elif a.cmd == 'repeat':
        if a.period_ms is None: p.error('repeat needs --period-ms')
        res.update(repeat(x, sr, a.onset_s, a.period_ms, [float(v) for v in a.freqs.split(',')]))
    elif a.cmd == 'thd':
        res.update(thd(x[int(a.start_s * sr):int(a.end_s * sr) if a.end_s else len(x)], sr, a.f0))
        if a.max_thd_db is not None: res['ok'] = res['thd_db'] <= a.max_thd_db
        if a.max_inharmonic_db is not None: res['ok'] = res['ok'] and res['inharmonic_db'] <= a.max_inharmonic_db
    elif a.cmd == 'alias':
        if a.fp is None: p.error('alias needs --fp')
        res.update(alias(x[int(a.start_s * sr):int(a.end_s * sr) if a.end_s else len(x)], sr, a.f0 or 1000.0, a.fp, a.probe_amp, a.all_frames))
        if a.max_alias_db is not None and 'alias_db_max' in res: res['ok'] = res['alias_db_max'] <= a.max_alias_db
    elif a.cmd == 'bursts':
        if a.period_ms is None: p.error('bursts needs --period-ms')
        res.update(bursts(x, sr, a.onset_s, a.period_ms, a.max_taps, a.skip_first))
    print(json.dumps(res)); return 0 if res['ok'] else 1

if __name__ == '__main__':
    sys.exit(main())
