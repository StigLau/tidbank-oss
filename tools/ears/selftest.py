#!/usr/bin/env python3
"""Proves every analyzer on synthetic signals with known answers, independent of any engine.
If an engine test fails later, the measurement is not the suspect. Run: uv run --with numpy tools/ears/selftest.py"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import analyze as A

SR = 48000; fails = []
def check(name, ok, detail=''):
    print(('PASS ' if ok else 'FAIL ') + name + (' ' + detail if detail else '')); ok or fails.append(name)

# --- taps: impulses at known fractional positions -> sub-sample timing
x = np.zeros(SR * 2)
for k, ms in enumerate([250.0, 500.0, 750.0]):
    pos = 0.1 * SR + ms * SR / 1000; i = int(pos); fr = pos - i
    x[i] += 0.5 ** k * (1 - fr); x[i + 1] += 0.5 ** k * fr   # linear-interp impulse at pos
t = [pp for pp, _ in A.taps(x, SR)]
err = [abs((t[k] - 0.1 * SR) * 1000 / SR - 250.0 * (k + 1)) for k in range(3)]
check('taps sub-sample timing', len(t) == 3 and max(err) < 0.02, f'max err {max(err):.4f} ms')

# --- spectrum: tap k = impulse through (k-1) extra one-pole lowpasses at 4 kHz
a_lp = 1 - np.exp(-2 * np.pi * 4000 / SR)
def lp(v):
    y = np.zeros_like(v); s = 0.0
    for i, u in enumerate(v): s += a_lp * (u - s); y[i] = s
    return y
def H(f):  # digital one-pole magnitude
    w = 2 * np.pi * f / SR; return abs(a_lp / (1 - (1 - a_lp) * np.exp(-1j * w)))
def bandH(f, k):
    g = np.linspace(f / 2 ** (1/6), f * 2 ** (1/6), 400); return 10 * np.log10(np.mean([H(v) ** (2 * k) for v in g]))
x = np.zeros(SR); pos = []
for k in range(3):
    imp = np.zeros(2048); imp[0] = 0.5 ** k
    for _ in range(k): imp = lp(imp)
    s = int(0.1 * SR + k * 0.25 * SR); x[s:s + 2048] += imp; pos.append(s + int(np.argmax(np.abs(imp))))
fl = [100.0, 1000.0, 10000.0]; sh = A.tap_shapes(x, SR, pos, fl, win_ms=20.0)
for k in (1, 2):
    for j, f in enumerate(fl):
        want = bandH(f, k) - bandH(1000.0, k)   # same 1/3-octave power averaging as the analyzer
        check(f'spectrum tap{k+1} shape @ {int(f)} Hz', abs(sh[k, j] - want) < 0.6, f'got {sh[k, j]:.2f} want {want:.2f} dB')

# --- wow/flutter: 3 kHz carrier, 1 Hz wow 0.2 % peak, 10 Hz flutter 0.05 % peak
T = 10.0; t = np.arange(int(T * SR)) / SR
ph = 2 * np.pi * 3000 * (t - 0.002 * np.cos(2 * np.pi * 1.0 * t) / (2 * np.pi * 1.0) - 0.0005 * np.cos(2 * np.pi * 10 * t) / (2 * np.pi * 10))
r = A.wow_flutter(np.sin(ph), SR)
check('wow rms', abs(r['wow_rms_pct'] - 0.2 / np.sqrt(2)) < 0.01, f"got {r['wow_rms_pct']} want {0.2/np.sqrt(2):.4f}")
check('flutter rms', abs(r['flutter_rms_pct'] - 0.05 / np.sqrt(2)) < 0.005, f"got {r['flutter_rms_pct']} want {0.05/np.sqrt(2):.4f}")
check('carrier', abs(r['carrier_hz'] - 3000) < 1, str(r['carrier_hz']))
check('clean tone has ~zero wow/flutter', A.wow_flutter(np.sin(2 * np.pi * 3000 * t), SR)['wow_rms_pct'] < 0.001)

# --- glide: ratio 1 -> 1.1, first order tau 0.3 s starting at 1 s. Settle (|r-final|<0.1 %) at 1 + 0.3*ln(0.1/0.0011)
T = 5.0; t = np.arange(int(T * SR)) / SR
ratio = 1 + 0.1 * (1 - np.exp(-np.clip(t - 1.0, 0, None) / 0.3)); ph = 2 * np.pi * 1000 * np.cumsum(ratio) / SR
g = A.glide(np.sin(ph), SR); want = 1 + 0.3 * np.log(0.1 / 0.0011)
check('glide final ratio', abs(g['final_ratio'] - 1.1) < 0.002, str(g['final_ratio']))
check('glide min/max', abs(g['min_ratio'] - 1.0) < 0.002 and abs(g['max_ratio'] - 1.1) < 0.003, f"{g['min_ratio']} {g['max_ratio']}")
check('glide settle time', abs(g['settle_s'] - want) < 0.08, f"got {g['settle_s']} want {want:.2f}")

# --- glide area: gaussian pitch bump 5 % high, sigma 0.2 s -> area = 0.05*0.2*sqrt(2*pi) = 0.02507 s
T = 6.0; t = np.arange(int(T * SR)) / SR
ratio = 1 + 0.05 * np.exp(-0.5 * ((t - 3.0) / 0.2) ** 2); ph = 2 * np.pi * 1000 * np.cumsum(ratio) / SR
g = A.glide(np.sin(ph), SR); check('glide area invariant', abs(g['area_s'] - 0.02507) < 0.0015, f"got {g['area_s']}")

# --- thd: 1 kHz, H2 -60 dB, H3 -50 dB -> THD = sqrt(1e-6 + 1e-5) = -49.6 dB
t = np.arange(SR) / SR
x = np.sin(2 * np.pi * 1000 * t) + 1e-3 * np.sin(2 * np.pi * 2000 * t) + 10 ** (-50 / 20) * np.sin(2 * np.pi * 3000 * t)
d = A.thd(x, SR, 1000.0)
check('thd total', abs(d['thd_db'] - (-49.59)) < 0.3, f"got {d['thd_db']}")
check('thd h2/h3', abs(d['h2_db'] + 60) < 0.3 and abs(d['h3_db'] + 50) < 0.3, f"h2 {d['h2_db']} h3 {d['h3_db']}")
check('thd pure sine floor', A.thd(np.sin(2 * np.pi * 1000 * t), SR, 1000.0)['thd_db'] < -120)

# --- health
x = np.zeros(1000, np.float32); x[5] = np.nan
check('health catches NaN', not A.health(x)['ok'])
x = np.zeros(1000, np.float32); x[5] = 1e-39
check('health catches subnormal', not A.health(x)['ok'] and A.health(x)['subnormals'] == 1)
check('health passes clean', A.health(np.sin(np.arange(1000) / 10).astype(np.float32))['ok'])

# --- step: two cascaded one-poles, 99 % settle at 300 ms (x = 6.638 time constants), no overshoot
a1 = np.exp(-6.638 / (0.3 * SR)); u = np.r_[np.ones(SR), np.full(2 * SR, 1.25)]; y1 = np.empty_like(u); y2 = np.empty_like(u); s1 = s2 = 1.0
for i, v in enumerate(u): s1 = v + a1 * (s1 - v); s2 = s1 + a1 * (s2 - s1); y1[i] = s1; y2[i] = s2
st = A.step(y2.astype(np.float32), SR, 1.0)
check('step settle/overshoot', abs(st['settle99_ms'] - 300.0) < 1.0 and st['overshoot_rel'] <= 1e-6, f"settle {st['settle99_ms']} ms, overshoot {st['overshoot_rel']:.2e}")
# --- thd inharmonic: sine + -60 dB tone at 1234.5 Hz -> -60 dB; pure sine < -120
t = np.arange(SR) / SR; sn = np.sin(2 * np.pi * 1000 * t)
d = A.thd(sn + 1e-3 * np.sin(2 * np.pi * 1234.5 * t), SR, 1000.0)
check('thd inharmonic -60 dB tone', abs(d['inharmonic_db'] + 60) < 1.0, f"got {d['inharmonic_db']}")
check('thd inharmonic pure sine floor', A.thd(sn, SR, 1000.0)['inharmonic_db'] < -120, str(A.thd(sn, SR, 1000.0)['inharmonic_db']))

# --- bursts: synthetic 20 ms bursts (2 ms cosine fades) every 250 ms at gains 0, -6.02, -12.04 dB
x = np.zeros(SR * 2); L = int(0.02 * SR); fd = int(0.002 * SR); env = np.ones(L)
env[:fd] = 0.5 - 0.5 * np.cos(np.pi * np.arange(fd) / fd); env[-fd:] = env[:fd][::-1]
for k, g in enumerate([0.0, -6.02, -12.04]):
    s0 = int(0.1 * SR + k * 0.25 * SR); x[s0:s0 + L] += 0.5 * 10 ** (g / 20) * env * np.sin(2 * np.pi * 1000 * np.arange(L) / SR)
b = A.bursts(x, SR, 0.1, 250.0, 2)
err = [abs((b['db'][k] - b['db'][0]) - g) for k, g in enumerate([0.0, -6.02, -12.04])]
check('bursts gain sequence', len(b['db']) == 3 and max(err) < 0.05, f"db {b['db']} step {b['step_db']}")
b1 = A.bursts(x, SR, 0.1, 250.0, 2, skip_first=True)
check('bursts skip-first', b1['k'] == [1, 2] and abs(b1['step_db'][0] + 6.02) < 0.05, str(b1))

# --- spectrum, --win-ms 100: tap k = impulse through k chained 2nd-order Butterworth highpasses at 80 Hz
def hp_coef(fc, q=2 ** -0.5):
    w0 = 2 * np.pi * fc / SR; al = np.sin(w0) / (2 * q); c = np.cos(w0); a0 = 1 + al
    return np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2]) / a0, np.array([1, -2 * c, 1 - al]) / a0
def hp(v, fc=80.0):
    b_, a_ = hp_coef(fc); y = np.zeros_like(v); x1 = x2 = y1 = y2 = 0.0
    for i, u in enumerate(v):
        o = b_[0] * u + b_[1] * x1 + b_[2] * x2 - a_[1] * y1 - a_[2] * y2; x2 = x1; x1 = u; y2 = y1; y1 = o; y[i] = o
    return y
def HH(f):
    b_, a_ = hp_coef(80.0); z = np.exp(-1j * 2 * np.pi * f / SR)
    return abs((b_[0] + b_[1] * z + b_[2] * z * z) / (1 + a_[1] * z + a_[2] * z * z))
def bandHH(f, k):
    g = np.linspace(f / 2 ** (1/6), f * 2 ** (1/6), 400); return 10 * np.log10(np.mean([HH(v) ** (2 * k) for v in g]))
x = np.zeros(SR * 2); pos = []
for k in range(4):
    imp = np.zeros(8192); imp[0] = 1.0
    for _ in range(k): imp = hp(imp)
    s0 = int(0.1 * SR + k * 0.3 * SR); x[s0:s0 + 8192] += imp; pos.append(s0 + int(np.argmax(np.abs(imp))))
sh = A.tap_shapes(x, SR, pos, [100.0], win_ms=100.0)
for k in (1, 2, 3):
    want = bandHH(100.0, k) - bandHH(1000.0, k)
    check(f'spectrum win 100 ms, {k}x HPF 80 Hz @ 100 Hz', abs(sh[k, 0] - want) < 0.2 * k, f'got {sh[k, 0]:.2f} want {want:.2f} dB')

# --- alias: pilot 1 kHz + probe 3450 Hz (0.25 each), both FM-glided ratio 1 -> 1.1 -> 1 over 6 s
T = 6.0; t = np.arange(int(T * SR)) / SR; ratio = 1 + 0.1 * np.sin(np.pi * t / T) ** 2; cs = np.cumsum(ratio) / SR
tt = lambda f: 0.25 * np.sin(2 * np.pi * f * cs)
x = tt(1000.0) + tt(3450.0)
r = A.alias(x, SR, 1000.0, 3450.0, 0.25)
check('alias clean two-tone glide', r['alias_db_max'] < -100 and r['frames_used'] > 50, str(r))
check('alias ratio range', abs(r['ratio_max'] - 1.1) < 0.005 and r['ratio_min'] >= 1.0, f"{r['ratio_min']} {r['ratio_max']}")
x2 = x + 0.25 * 10 ** (-40 / 20) * np.sin(2 * np.pi * 6750.0 * t)
r = A.alias(x2, SR, 1000.0, 3450.0, 0.25)
check('alias -40 dB inharmonic', abs(r['alias_db_max'] + 40) < 1 and abs(r['alias_db_median'] + 40) < 1, str(r))
x3 = tt(1000.0) + tt(10000.0) + 0.25 * 10 ** (-30 / 20) * np.sin(2 * np.pi * 500.0 * t)
r = A.alias(x3, SR, 1000.0, 10000.0, 0.25)
check('alias 10 kHz probe, alias at 500 Hz (-30 dB) detected', abs(r['alias_db_max'] + 30) < 1, str(r))
x4 = tt(1000.0) + tt(10000.0) + 0.25 * 10 ** (-30 / 20) * np.sin(2 * np.pi * 150.0 * t)
r = A.alias(x4, SR, 1000.0, 10000.0, 0.25)
check('alias below 200 Hz floor ignored', r['alias_db_max'] < -100, str(r['alias_db_max']))
r = A.alias(np.sin(2 * np.pi * 1000.0 * np.arange(SR * 2) / SR) * 0.25 + 0.25 * np.sin(2 * np.pi * 3450.0 * np.arange(SR * 2) / SR), SR, 1000.0, 3450.0, 0.25, all_frames=True)
check('alias static, all-frames', r['alias_db_max'] < -100 and r['frames_used'] > 20, str(r))
st = A.stats(np.array([0.5, -2.0, 1.0], dtype=np.float32))
check('stats', st['min'] == -2.0 and st['max'] == 1.0 and st['max_abs'] == 2.0 and abs(st['mean'] + 1 / 6) < 1e-9, str(st))
# --- click: 500 Hz tone at -20 dBFS switching phase hard (click) vs 20 ms equal-power crossfade (smooth)
n = 3 * SR; t = np.arange(n) / SR; a_ = 0.1 * np.sin(2 * np.pi * 500 * t); b_ = 0.1 * np.sin(2 * np.pi * 500 * t + 2.0)
hard = np.where(t < 1.5, a_, b_); g = np.clip((t - 1.5) / 0.02, 0, 1); soft = np.cos(g * np.pi / 2) * a_ + np.sin(g * np.pi / 2) * b_
ch, cs = A.click(hard, SR, 1.5), A.click(soft, SR, 1.5)
check('click hard switch detected', ch['click_db'] > -40, str(ch))
check('click crossfade quiet', cs['click_db'] < -60 and cs['baseline_db'] < -100, str(cs))
print('SELFTEST ' + ('FAILED: ' + ', '.join(fails) if fails else 'OK'))
sys.exit(1 if fails else 0)
