#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy"]
# ///
"""Ears suite: table of cases (render args, analyzer, judgement). Usage: suite.py [case-name-prefix].
Run via: uv run --quiet --locked --script tools/ears/suite.py [prefix]   (or tools/ears/run_ears.sh / make ears CASE=prefix)
Scratch output goes to ${TMPDIR}/tidbank-ears, never the repo. Exit 1 if any case fails."""
import json, os, subprocess, sys, time
from dataclasses import dataclass
from typing import Callable, List, Optional

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
RENDER = os.path.join(ROOT, 'build', 'tidbank_render')
RENDER_PROBE = os.path.join(ROOT, 'build', 'tidbank_render_probe')  # same renderer, debug probe compiled in
ANALYZE = os.path.join(ROOT, 'tools', 'ears', 'analyze.py')
SELFTEST = os.path.join(ROOT, 'tools', 'ears', 'selftest.py')
OUT = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'tidbank-ears')

TTY = sys.stdout.isatty()
def col(code, s): return f'\033[{code}m{s}\033[0m' if TTY else s

Judge = Callable[[dict], "tuple[bool, str]"]

@dataclass
class Case:
    name: str
    judge: Judge
    render: Optional[List[str]] = None    # tidbank_render args; '{wav}' is replaced by the output path
    analyze: Optional[List[str]] = None   # analyze.py args; '{wav}' is replaced by the output path
    script: Optional[str] = None          # run this script instead (selftest): last stdout line goes to judge as {'line':...}
    multi: Optional[list] = None          # list of (render, analyze) pairs; judge gets the list of results
    known: Optional[str] = None           # known failure tied to a recorded known issue: reported KNOWN/FIXED?, not counted
    fn: Optional[Callable[[], "tuple[bool, str]"]] = None  # run this directly (probe cases: several renders + CSV/WAV checks)

def ok_flag(extra=None):
    """Judge: analyzer's own ok flag, plus optional detail keys to print."""
    def j(r):
        det = ' '.join(f'{k}={r[k]}' for k in (extra or []) if k in r)
        return bool(r.get('ok')), det
    return j

def selftest_judge(r):
    return r['line'].startswith('SELFTEST OK'), r['line']

def sync_case(bpm):
    exp = 0.5 * 60000 / bpm
    return Case(f'sync_{bpm}',
                render=['--out', '{wav}', '--bpm', str(bpm), '--div', '0.5', '--fb', '0.5', '--dry', '0', '--secs', '6'],
                analyze=['taps', '{wav}', '--expect-ms', repr(exp), '--tol-ms', '0.1'],
                judge=lambda r, exp=exp: (bool(r.get('ok')), f"expect={exp:g}ms err_ms={r.get('err_ms')}"))

TONE = ['--out', '{wav}', '--stim', 'tone', '--freq', '1000', '--level', '0.5', '--dry', '1', '--wet', '0', '--secs', '2']

# ---- S2: exact tape transport, single head (plan-tape-engine.md S2). Head ratios even 1:2:3, D1 = 0.5 s.
TAPE = ['--engine', 'tape', '--dry', '0']
def near(x, want, rel): return abs(x - want) <= rel * abs(want)

def s2_taps(name, bpm, head, sr=48000, nudge=0.0):
    exp = 0.5 * 60000 / bpm * head * 2 ** (-nudge / 1200)
    return Case(name, render=TAPE + ['--out', '{wav}', '--sr', str(sr), '--bpm', repr(bpm), '--div', '0.5',
                                      '--params', f'head={head},nudge_cents={nudge},intensity=0', '--secs', str(0.5 + exp / 1000 * 1.2)],
                analyze=['taps', '{wav}', '--expect-ms', repr(exp), '--tol-ms', '0.01', '--max-taps', '1'],
                judge=lambda r, exp=exp: (bool(r.get('ok')), f"expect={exp:.4f}ms err_ms={r.get('err_ms')}"))

def s2_glide(name, head, up, inertia=300.0, check_peak=True):
    # 500 -> 450 ms on head 1 (v 1 -> 1.111) at t = 3 s, or back. Tone through one solo head.
    b0, b1 = (120.0, 400 / 3) if up else (400 / 3, 120.0)
    tau_old, tau_new = 0.5 * head * 120 / b0, 0.5 * head * 120 / b1
    area, speed = tau_old - tau_new, b1 / b0
    def judge(r):
        ok = near(r['area_s'], area, 0.02)
        if check_peak:  # the exact model's peak pitch equals the speed ratio on every head; quasi-static overshoots 1.4-2.3x
            ok &= near(r['max_ratio'] if up else r['min_ratio'], speed, 0.01)
        # glide lasts tau_new + ~inertia (exact model); quasi-static ends with the speed change (~inertia) on every head
        dur = r['settle_s'] - 1.0
        if check_peak: ok &= abs(dur - tau_new - inertia / 1000) <= 0.05 + 0.02 * inertia / 1000
        return ok, (f"area={r['area_s']} want {area:.4f} peak={r['max_ratio'] if up else r['min_ratio']} want {speed:.5f} "
                    f"dur={dur:.3f} want {tau_new + inertia / 1000:.3f}")
    return Case(name, render=TAPE + ['--out', '{wav}', '--stim', 'tone', '--freq', '1000', '--level', '0.05', '--bpm', repr(b0),  # linear region of the saturator
                                    
                                     '--div', '1.0', '--params', f'head={head},inertia_ms={inertia},intensity=0', '--sched', f'3.0:bpm={b1!r}',
                                     '--secs', '9'],
                analyze=['glide', '{wav}', '--f0', '1000', '--start-s', '2.0', '--end-s', '8.9'], judge=judge)

def s2_vstep(name, inertia):
    def judge(r):
        return (r['overshoot_rel'] <= 1e-6 and near(r['settle99_ms'], inertia, 0.02),
                f"settle99={r['settle99_ms']}ms want {inertia} overshoot={r['overshoot_rel']:.1e}")
    return Case(name, render=['--engine', 'tape', '--probe', 'v', '--out', '{wav}', '--bpm', '120', '--div', '1.0',
                              '--params', f'inertia_ms={inertia}', '--sched', '3.0:bpm=133.33333333', '--secs', '9'],
                analyze=['step', '{wav}', '--t-step', '3.0'], judge=judge)

# ---- S4: head_target x spacing. Each head lands at ratio_i * tau_target / ratio_target.
RATIOS = {0: (1.0, 2.0, 3.0), 1: (1.0, 1.9, 2.75)}
def s4_taps(target, spacing, head):
    r = RATIOS[spacing]; exp = r[head - 1] * 250.0 / r[target - 1]
    name = f's4_taps_t{target}_{"even" if spacing == 0 else "vint"}_h{head}'
    return Case(name, render=TAPE + ['--out', '{wav}', '--bpm', '120', '--div', '0.5', '--secs', str(0.5 + exp / 1000 * 1.2),
                                      '--params', f'head={head},head_target={target},spacing={spacing},intensity=0'],
                analyze=['taps', '{wav}', '--expect-ms', repr(exp), '--tol-ms', '0.01', '--max-taps', '1'],
                judge=lambda r, exp=exp: (bool(r.get('ok')), f"expect={exp:.4f}ms err_ms={r.get('err_ms')}"))
S4: List[Case] = [s4_taps(t, sp, h) for t in (1, 2, 3) for sp in (0, 1) for h in (1, 2, 3)]

def s2_alias(name, fp, up, max_median_db):
    b0, b1 = (120.0, 150.0) if up else (150.0, 120.0)   # head 1: 500 <-> 400 ms, ratio 1.25 / 0.8
    def judge(r):
        # median is asserted; max is reported: the analyzer reads -49.7 dB max on an IDEAL resampler at glide onset
        return r.get('alias_db_median', 0) <= max_median_db, f"median={r.get('alias_db_median')} max={r.get('alias_db_max')} want median<={max_median_db}"
    return Case(name, render=TAPE + ['--out', '{wav}', '--stim', 'twotone', '--freq', '1000', '--level', '0.05', '--freq2', str(fp),
                                     '--level2', '0.05', '--bpm', repr(b0), '--div', '1.0', '--params', 'head=1,intensity=0',
                                     '--sched', f'3.0:bpm={b1!r}', '--secs', '6'],
                analyze=['alias', '{wav}', '--f0', '1000', '--fp', str(fp), '--probe-amp', '0.05', '--start-s', '1.0'], judge=judge)

def probe_case(name, probe, params, judge_fn, desc, secs='3', sched=None, start='0.0', end=None):
    r = ['--engine', 'tape', '--probe', probe, '--out', '{wav}', '--params', params, '--secs', secs] + (['--sched', sched] if sched else [])
    an = ['stats', '{wav}', '--start-s', start] + (['--end-s', end] if end else [])
    return Case(name, render=r, analyze=an, judge=lambda res: (judge_fn(res), f"{desc}: {res}"))

# ---- S3: feedback network (HPF, LPF, saturator, intensity) at the record head
def s3_cases():
    import math
    imp = TAPE + ['--out', '{wav}', '--bpm', '120', '--div', '0.5', '--secs', '2.2']
    def gd_judge(r):
        e = r.get('err_ms', []); gd = e[-1] / (len(e) - 1) if len(e) > 6 else 99
        lin = all(abs(e[k] - k * gd) < 0.01 for k in range(len(e)))
        return 0 <= gd < 0.1 and lin, f'gd={gd:.4f} ms/repeat (want < 0.1, linear) err_ms={e}'
    cases = [Case('s3_loop_gd', render=imp + ['--params', 'head=1,intensity=0.7,drive=-12'],
                  analyze=['taps', '{wav}', '--expect-ms', '250', '--tol-ms', '1', '--max-taps', '7', '--min-gap-ms', '100'], judge=gd_judge)]
    def lvl_judge(r):
        st = r['step_db']; return len(st) >= 5 and all(abs(v + 6.02) <= 0.3 for v in st), f'step_db={st} want -6.02+-0.3'
    cases.append(Case('s3_burst_level', render=TAPE + ['--out', '{wav}', '--stim', 'burst', '--level', '0.25', '--bpm', '20', '--secs', '2.5',
                                                         '--params', 'sync=0,time_ms=250,head=1,intensity=0.5,drive=0'],
                      analyze=['bursts', '{wav}', '--period-ms', '250', '--max-taps', '6', '--skip-first'], judge=lvl_judge))
    for fc in (20, 80, 300):
        # analyzer averages linear FFT bins over the 1/3-octave band: expect the band mean of |H_hpf|^(2k) after k passes
        lo, hi = 100 * 2 ** (-1 / 6), 100 * 2 ** (1 / 6); fs = [lo + (hi - lo) * i / 2000 for i in range(2001)]
        exp = [10 * math.log10(sum((1 / (1 + (fc / f) ** 4)) ** k for f in fs) / len(fs)) for k in range(6)]
        def sp_judge(r, exp=exp):
            col = [row[0] for row in r['shape_db_vs_tap1']]
            ok = len(col) >= 4 and all(abs(col[k] - exp[k]) <= 0.3 * max(k, 1) for k in range(len(col)))
            return ok, f'100 Hz vs tap 1: {col} want {[round(e, 2) for e in exp[:len(col)]]} +-0.3/repeat'
        cases.append(Case(f's3_hpf_{fc}', render=imp[:-1] + ['1.6', '--params', f'head=1,intensity=0.7,drive=-12,fb_hpf={fc}'],
                          analyze=['spectrum', '{wav}', '--freqs', '100,1000', '--win-ms', '100', '--min-gap-ms', '100', '--max-taps', '5'],
                          judge=sp_judge))
    drives = (-12, 0, 6, 12, 18)
    def drv_judge(rs):
        thd = [r['thd_db'] for r in rs]; h3 = all(r['h3_db'] > r['h2_db'] for r in rs); inh = rs[-1]['inharmonic_db']
        ok = all(b > a for a, b in zip(thd, thd[1:])) and h3 and inh < -45
        return ok, f'thd {dict(zip(drives, thd))} H3>H2={h3} inharmonic@+18={inh} (want monotonic, < -45)'
    cases.append(Case('s3_drive_thd', judge=drv_judge, multi=[
        (TAPE + ['--out', '{wav}', '--stim', 'tone', '--freq', '1997', '--level', '0.5', '--bpm', '120', '--div', '0.5', '--secs', '3',
                 '--params', f'head=1,intensity=0,drive={d}'],
         ['thd', '{wav}', '--f0', '1997', '--start-s', '0.5', '--end-s', '2.9']) for d in drives]))
    run60 = TAPE + ['--out', '{wav}', '--stim', 'noise', '--level', '0.25', '--bpm', '120', '--div', '0.5', '--secs', '60',
                    '--params', 'intensity=1.2,mode=7']
    cases.append(Case('s3_runaway_bounded', render=run60, analyze=['stats', '{wav}', '--windows', '10:30,30:60'],
                      judge=lambda r: (r['window_peak_db'][1] <= r['window_peak_db'][0] + 1.0, f"peak 10-30 s / 30-60 s dBFS: {r['window_peak_db']}")))
    cases.append(Case('s3_runaway_health', render=run60, analyze=['health', '{wav}'], judge=ok_flag(['peak_dbfs', 'subnormals'])))
    # loop gain <= intensity in EVERY mode (feedback sum normalised by active head weight): one burst at t=0 must die out
    for m in (4, 7):
        cases.append(Case(f's3_loopgain_mode{m}', render=TAPE + ['--out', '{wav}', '--stim', 'burst', '--level', '0.25', '--bpm', '5', '--secs', '10',
                                                                   '--params', f'sync=0,time_ms=250,mode={m},intensity=0.9,drive=0'],
                          analyze=['stats', '{wav}', '--windows', '0.2:2,8:10'],
                          judge=lambda r: (r['window_peak_db'][1] <= r['window_peak_db'][0] - 6, f"peak 0.2-2 s / 8-10 s dBFS: {r['window_peak_db']} want late <= early-6 (unnormalised: +11 / +1 dB, grows)")))
    return cases

# ---- S4 extras: switches must not click (high-pass 2.5 kHz, peak +-50 ms of the switch, 500 Hz tone at -20 dBFS)
def s4_click(name, params, sched):
    return Case(name, render=TAPE + ['--out', '{wav}', '--stim', 'tone', '--freq', '500', '--level', '0.1', '--bpm', '120', '--div', '0.5',
                                     '--params', params + ',intensity=0', '--sched', sched, '--secs', '3'],
                analyze=['click', '{wav}', '--t-step', '2.0', '--max-click-db', '-60'], judge=ok_flag(['click_db', 'baseline_db']))
S4X: List[Case] = [
    s4_click('s4_click_mode_1_4', 'mode=1', '2.0:mode=4'), s4_click('s4_click_mode_1_7', 'mode=1', '2.0:mode=7'),
    s4_click('s4_click_mode_7_12', 'mode=7', '2.0:mode=12'), s4_click('s4_click_spacing_h3', 'head=3', '2.0:spacing=1'),
    s4_click('s4_click_target_h3', 'head=3', '2.0:head_target=2'), s4_click('s4_click_tempo_h3', 'head=3', '2.0:bpm=90'),
    s4_click('s4_click_nudge_h3', 'head=3', '2.0:nudge_cents=400'),
    *[Case(f's4_mode{m}_health', render=TAPE + ['--out', '{wav}', '--stim', 'noise', '--level', '0.25', '--bpm', '120', '--div', '0.5',
                                                 '--secs', '30', '--params', f'mode={m},intensity=1.0'],
           analyze=['health', '{wav}'], judge=ok_flag(['peak_dbfs'])) for m in range(1, 13)],
]

# ---- S7: 5-minute renders at extreme settings with tempo jumps, nudge sweeps and mode changes
def s7_sched(secs=300):
    ev = {}
    for i in range(1, secs // 2):  # tempo jump every 2 s, deterministic 60..180 BPM
        ev.setdefault(2.0 * i, []).append(f'bpm={60 + (i * 37) % 121}')
    for i in range(1, secs):       # nudge sweep, +-400 c triangle with 8 s period
        ph = (i % 8) / 8; ev.setdefault(float(i), []).append(f'nudge_cents={400 * (4 * abs(ph - 0.5) - 1):.1f}')
    for i in range(1, secs // 3):  # mode change every 3 s, cycling 1..12
        ev.setdefault(3.0 * i, []).append(f'mode={1 + i % 12}')
    return ';'.join(f'{t}:' + ','.join(v) for t, v in sorted(ev.items()))
S7_PARAMS = 'intensity=1.2,drive=18,inertia_ms=30'
S7_RENDER = TAPE + ['--out', '{wav}', '--stim', 'noise', '--level', '0.25', '--bpm', '120', '--div', '0.5', '--secs', '300',
                    '--params', S7_PARAMS, '--sched', s7_sched()]
S7: List[Case] = [
    Case('s7_extreme_health', render=S7_RENDER, analyze=['health', '{wav}'],
         judge=lambda r: (r['ok'], f"finite={r['finite']} subnormals={r['subnormals']} peak_dbfs={r['peak_dbfs']} render={r['_render_s']:.2f}s "
                                   f"for 300 s mono = {100 * r['_render_s'] / 300:.2f} % of real time (budget 5 %)")),
    Case('s7_extreme_bounded', render=S7_RENDER, analyze=['stats', '{wav}', '--windows', '30:150,150:300'],
         judge=lambda r: (r['window_peak_db'][1] <= r['window_peak_db'][0] + 1.0, f"peak 30-150 / 150-300 s dBFS: {r['window_peak_db']}")),
    Case('s7_cpu', render=S7_RENDER, analyze=['health', '{wav}'],
         judge=lambda r: (r['_render_s'] < 0.05 * 300, f"render {r['_render_s']:.2f}s for 300 s (< 15 s = 5 % real time)")),
]

S2: List[Case] = [
    s2_taps('s2_taps_h1_120', 120.0, 1), s2_taps('s2_taps_h2_120', 120.0, 2), s2_taps('s2_taps_h3_120', 120.0, 3),
    s2_taps('s2_taps_h1_90', 90.0, 1), s2_taps('s2_taps_h1_128', 128.0, 1),
    s2_taps('s2_taps_h1_97', 97.0, 1), s2_taps('s2_taps_h3_97_44k', 97.0, 3, sr=44100),   # fractional delays
    s2_taps('s2_nudge_up100', 120.0, 1, nudge=100.0), s2_taps('s2_nudge_dn400_h3', 120.0, 3, nudge=-400.0),
    *[s2_glide(f's2_glide_{"up" if up else "dn"}_h{h}', h, up) for up in (True, False) for h in (1, 2, 3)],
    s2_glide('s2_glide_inertia30_h3', 3, True, inertia=30.0),
    s2_glide('s2_glide_inertia2000_h3', 3, True, inertia=2000.0, check_peak=False),  # peak < ratio when inertia > tau
    s2_vstep('s2_vstep_300', 300.0), s2_vstep('s2_vstep_30', 30.0), s2_vstep('s2_vstep_2000', 2000.0),
    *[s2_alias(f's2_alias_{fp // 1000}k_{"up" if up else "dn"}', fp, up, -60.0) for fp in (5000, 15000) for up in (True, False)],
    Case('s2_inharm_static', render=TAPE + ['--out', '{wav}', '--stim', 'tone', '--freq', '1997', '--level', '0.05', '--bpm', '97',
                                             '--div', '0.5', '--params', 'head=1,intensity=0', '--secs', '3'],
         analyze=['thd', '{wav}', '--f0', '1997', '--start-s', '0.5', '--end-s', '2.9', '--max-inharmonic-db', '-85'],
         judge=ok_flag(['inharmonic_db', 'thd_db'])),
    *[probe_case(f's2_tauerr{h}_settled', f'tauerr{h}', 'bpm=97,division=0.5', lambda r: r['max_abs'] < 1e-3,
                 'steady tau - d/v < 1e-3 samples, after a 97->131 BPM step', secs='4', sched='1.0:bpm=131', start='3.0') for h in (1, 2, 3)],
    probe_case('s2_clamped_yes', 'clamped', 'bpm=180,division=0.125,head_target=3', lambda r: r['min'] == 1.0, 'head 3 at 1/32 @180 needs v=36', secs='0.1'),
    probe_case('s2_clamped_no', 'clamped', 'bpm=120,division=0.5', lambda r: r['max'] == 0.0, '1/8 @120 in range', secs='0.1'),
]

# ---- B: BBD engine against its profiles (written before the engine).
# Expectations come from `tidbank_render --dump-profile all`: the profile's facts and its designed filter response.
# LTI checks run with comp=0, noise=0, nonlin=0. Only engine behaviour is judged here; whether a profile's numbers
# match the real unit is the profile's status (verified / inferred / placeholder), shown in the case detail.
# Known failures, by case name, each tied to a recorded known issue. A known case that passes prints FIXED? and FAILS the run, so
# the entry gets removed on purpose.
KNOWN = {
    # Loop gain is normalised per profile since 2026-11-16 (fb x loopNorm). What is left is the wet (forward) path: one pass
    # through the chain is still |H|^2 = +23.6 dB at 1.96 kHz, and the hold droop at 200 ms leaves d4096 below 1 at fb 1.1.
    'b7_health_d4096': 'normalised loop at 200 ms (10.2 kHz clock) is 1.1 x ~0.88 hold droop < 1: decays, no oscillation to judge',
    'b7_health_c8192': 'oscillation bounded but at ~+20 dBFS: the wet path keeps the chain\'s +23.6 dB forward peak (filter values)',
}

def bbd_cases():
    import math
    try:
        profs = json.loads(subprocess.run([RENDER, '--dump-profile', 'all'], capture_output=True, text=True, check=True).stdout)
    except Exception as e:
        return [Case('b0_profiles', judge=lambda r, e=e: (False, f'--dump-profile failed: {e!r}'), render=['--dump-profile', 'all'], analyze=['health', '{wav}'])]
    cases = []
    for pf in profs:
        pid = pf['id']; lo, hi = pf['delay_min_ms']['v'], pf['delay_max_ms']['v']; N = pf['stages']['v']
        B = ['--engine', 'bbd', '--profile', pid, '--dry', '0']
        lti = 'fb=0,comp=0,noise=0,nonlin=0'
        # b1: echo peak lands at TIME + the filter chain's impulse-response peak; tolerance one BBD tick + 2 samples
        irpk = pf['sets'][0]['ir_peak_ms']
        for tag, T in (('min', lo), ('max', hi)):
            tol = 2 * T / N + 2000 / 48000  # ms: one tick (1/f_clk = 2T/N) + 2 samples at 48 kHz
            cases.append(Case(f'b1_delay_{pid}_{tag}',
                render=B + ['--out', '{wav}', '--params', f'time_ms={T},{lti}', '--secs', repr(T / 1000 + 0.6)],
                analyze=['taps', '{wav}', '--expect-ms', repr(T + irpk), '--tol-ms', repr(tol), '--max-taps', '1'],
                judge=lambda r, T=T, tol=tol, irpk=irpk, s=pf['delay_min_ms']['src'] + '/' + pf['delay_max_ms']['src']:
                    (bool(r.get('ok')), f"T={T:g}ms+ir {irpk:.3f} err_ms={r.get('err_ms')} tol {tol:.3f} ({s})")))
        # b2: the engine's clock satisfies t_d = N / (2 f_clk)
        cases.append(Case(f'b2_clock_{pid}',
            render=B + ['--out', '{wav}', '--probe', 'clk', '--params', f'time_ms={hi},{lti}', '--secs', '1'],
            analyze=['stats', '{wav}', '--start-s', '0.5'],
            judge=lambda r, N=N, hi=hi, src=pf['stages']['src']: (
                abs(2 * r.get('mean', 0) * hi / 1000 / N - 1) < 1e-3,
                f"f_clk={r.get('mean', 0):.1f}Hz -> N={2 * r.get('mean', 0) * hi / 1000:.1f} want {N:g} (stages {src})")))
        for si, st in enumerate(pf['sets']):
            bright = 1 - si
            # b3: filter-only response (noise in, LTI) matches the designed anti-alias x reconstruction response
            ref = os.path.join(OUT, f'b3_filter_{pid}_{st["name"]}_in.wav')
            cases.append(Case(f'b3_filter_{pid}_{st["name"]}',
                render=B + ['--out', '{wav}', '--write-in', ref, '--stim', 'noise', '--level', '0.05',
                            '--params', f'time_ms={lo},bright={bright},{lti}', '--secs', '6'],
                analyze=['tf', '{wav}', '--ref', ref],
                judge=lambda r, st=st, src=pf['filter_src']: (
                    r.get('f3_hz') is not None and abs(r['f3_hz'] / st['filter_f3_hz'] - 1) < 0.03
                    and all(abs(r['resp_db'][k] - st['resp_db'][k]) < 1.0 for k in ('500', '1000', '2000')),
                    f"f3={r.get('f3_hz')} want {st['filter_f3_hz']:g}; resp {r.get('resp_db')} ({src})")))
            # b4: wet bandwidth target, steady sine with the compander on (dB doubled): -3 dB at the target
            if st['wet_bw_hz']['v'] > 0:
                fw = st['wet_bw_hz']['v']
                tone = lambda f: (B + ['--out', '{wav}', '--stim', 'tone', '--freq', repr(f), '--level', '0.1',
                                       '--params', f'time_ms={lo},bright={bright},fb=0,comp=1,noise=0,nonlin=0', '--secs', '2.5'],
                                  ['level', '{wav}', '--start-s', '1.5', '--end-s', '2.4'])
                cases.append(Case(f'b4_wet_{pid}_{st["name"]}', multi=[tone(200), tone(fw)],
                    judge=lambda rs, fw=fw, src=st['wet_bw_hz']['src']: (
                        abs((rs[1]['rms_db'] - rs[0]['rms_db']) + 3.0) < 0.5,
                        f"{fw:g}Hz re 200Hz = {rs[1]['rms_db'] - rs[0]['rms_db']:.2f} dB want -3 ({src})")))
        # b5: compander tracks: a steady 200 Hz sine comes out at its own level over 26 dB (8.6)
        lv = [0.01, 0.05, 0.2]
        cases.append(Case(f'b5_unity_{pid}', multi=[
            (B + ['--out', '{wav}', '--stim', 'tone', '--freq', '200', '--level', repr(a),
                  '--params', f'time_ms={lo},fb=0,comp=1,noise=0,nonlin=0', '--secs', '2.5'],
             ['level', '{wav}', '--start-s', '1.5', '--end-s', '2.4']) for a in lv],
            judge=lambda rs, lv=lv: (
                all(abs(r['rms_db'] - 20 * math.log10(a / math.sqrt(2))) < 0.5 for r, a in zip(rs, lv)),
                'out-in dB ' + str([round(r['rms_db'] - 20 * math.log10(a / math.sqrt(2)), 2) for r, a in zip(rs, lv)]))))
        # b6: each repeat passes both filters again: tap 2 / tap 1 = fb x designed chain response x zero-order-hold droop
        # sinc(f / f_clk) of one more pass, at exact frequencies. Clock = fs/2 (48 kHz renders): every tick lands on a host
        # sample, so the simple path's hold aliasing (non-integer tick times) is absent and only the loop wiring is judged.
        # Aliasing is judged separately when the BLEP path exists (d18). fb is scaled by the profile's loop norm (the
        # compander is bypassed here, so the bypass scale = 1 / chain peak).
        st = pf['sets'][0]; d = st['resp_db']
        g200 = st['gain_200_db']; nb = pf['loop_norm']['bypass']
        def b6(name, T6, droop, tol):
            want6 = {k: 20 * math.log10(0.2 * nb) + d[k] + g200 + droop(float(k)) for k in ('500', '1000', '2000')}
            return Case(name,
                render=B + ['--out', '{wav}', '--params', f'time_ms={T6},fb=0.2,comp=0,noise=0,nonlin=0', '--secs', repr(T6 * 4 / 1000 + 0.3)],
                analyze=['repeat', '{wav}', '--period-ms', repr(T6), '--freqs', '500,1000,2000'],
                judge=lambda r, want6=want6, T6=T6, tol=tol, src=pf['fb_src']: (
                    all(abs(r['ratio_db'][k] - v) < tol for k, v in want6.items()),
                    f"T={T6:.1f}ms tap2/tap1 {r.get('ratio_db')} want {{{', '.join(f'{k}: {v:.2f}' for k, v in want6.items())}}} +-{tol} (feedback path {src})"))
        # 2-sample hold at fs/2: droop |cos(pi f / fs)|; wiring only, tight
        cases.append(b6(f'b6_repeat_{pid}', N / 48000 * 1000, lambda f: 20 * math.log10(abs(math.cos(math.pi * f / 48000))), 0.25))
        # ordinary clock (300 ms): hold droop sinc(f / f_clk). Tolerance 1 dB: the simple path's hold aliasing (ticks between
        # host samples) adds up to 0.9 dB at 500 Hz-1 kHz on the peaky DM-2 chain (measured 2026-11-16); the fs/2 case above
        # checks the wiring to 0.25 dB. Tighten when the BLEP path exists.
        T6b = min(max(300.0, lo), hi); fcb = N / (2 * T6b / 1000)
        cases.append(b6(f'b6_repeat_{pid}_300', T6b, lambda f, fcb=fcb: 20 * math.log10(abs(math.sin(math.pi * f / fcb) / (math.pi * f / fcb))), 1.0))
        # b7: self-oscillation stays bounded and clean (feedback above 1, compander on, noise in)
        r7 = B + ['--out', '{wav}', '--params', 'time_ms=200,fb=1.1', '--secs', '60']
        cases.append(Case(f'b7_health_{pid}', multi=[(r7, ['health', '{wav}']), (r7, ['stats', '{wav}', '--windows', '50:60'])],
            judge=lambda rs: (bool(rs[0].get('ok')) and -40.0 < rs[1]['window_peak_db'][0] < 12.0,
                              f"peak_dbfs={rs[0].get('peak_dbfs')} late(50-60s)={rs[1]['window_peak_db'][0]} subnormals={rs[0].get('subnormals')}"),
            known=KNOWN.get(f'b7_health_{pid}')))
        # b8: BBD noise at the chip's S/N (raised by at most the filter chain's peak); the expander pushes idle noise down
        nref = 20 * math.log10(1 / math.sqrt(2)) - pf['snr_db']['v']; npk = max(1.0, pf['sets'][0]['peak_db'])
        idle = lambda comp: (B + ['--out', '{wav}', '--stim', 'tone', '--level', '0', '--params', f'time_ms={hi},fb=0,comp={comp},noise=1', '--secs', '3'],
                             ['level', '{wav}', '--start-s', '1.5', '--end-s', '2.9'])
        cases.append(Case(f'b8_noise_{pid}', multi=[idle(0), idle(1)],
            judge=lambda rs, nref=nref, npk=npk, src=pf['snr_db']['src']: (
                nref - 6 < rs[0]['rms_db'] < nref + npk and rs[1]['rms_db'] < rs[0]['rms_db'] - 10,
                f"raw {rs[0]['rms_db']:.1f} dB (S/N ref {nref:.1f}, {src}), compander idle {rs[1]['rms_db']:.1f} dB")))
        # b9: RS10 nonlinearity: a 500 Hz sine of amplitude A gives H2 = a A / 2 (re fundamental), shaped by the chain at 1 kHz
        A = 0.5; aNl = pf['nl_a']['v']; want9 = 20 * math.log10(aNl * A / 2) + d['1000'] - d['500']
        cases.append(Case(f'b9_thd_{pid}',
            render=B + ['--out', '{wav}', '--stim', 'tone', '--freq', '500', '--level', repr(A),
                        '--params', f'time_ms={lo},fb=0,comp=0,noise=0,nonlin=1', '--secs', '2.5'],
            analyze=['thd', '{wav}', '--f0', '500', '--start-s', '1.0', '--end-s', '2.4'],
            judge=lambda r, want9=want9, src=pf['nl_a']['src']: (abs(r.get('h2_db', 0) - want9) < 1.5,
                f"H2={r.get('h2_db')} dB want {want9:.2f} H3={r.get('h3_db')} (a {src})")))
        # b10: Bright/Dark switch during a tone is click-free (tidbank-rt: anything that jumps cross-fades)
        if pf['has_dark']:
            for frm in (1, 0):
                cases.append(Case(f'b10_click_{pid}_{"b2d" if frm else "d2b"}',
                    render=B + ['--out', '{wav}', '--stim', 'tone', '--freq', '500', '--level', '0.1',
                                '--params', f'time_ms={lo},fb=0,bright={frm}', '--sched', f'2.0:bright={1 - frm}', '--secs', '3'],
                    analyze=['click', '{wav}', '--t-step', '2.0', '--max-click-db', '-60'], judge=ok_flag(['click_db', 'baseline_db'])))
    return cases

# ---- Debug probe: per-head delay trace. Records come from tidbank_render_probe --probe-csv.
# Judged two ways: |measured - computed| <= 1 sample on every record (the probe agrees with the engine's own delay), and, for
# solo tape heads, the echo's energy centroid in the WAV lands within 1 sample of the record's n_out (the probe agrees with the audio).
def probe_run(tag, args, interval=1.0, offset=0.1, secs=8.0, exe=None):
    import csv as csvmod
    os.makedirs(OUT, exist_ok=True)
    wav, cs = os.path.join(OUT, f'{tag}.wav'), os.path.join(OUT, f'{tag}.csv')
    p = subprocess.run([exe or RENDER_PROBE, '--out', wav, '--probe-csv', cs, '--probe-interval', repr(interval), '--probe-offset-s', repr(offset),
                        '--secs', repr(secs)] + args, capture_output=True, text=True)
    if p.returncode != 0: raise RuntimeError(f'render rc={p.returncode} {p.stderr.strip()[-200:]}')
    with open(cs) as f: rows = [r for r in csvmod.DictReader(f)]
    return wav, [r for r in rows if r['type'] == 'delay'], rows

def read_f32_wav(path):
    import numpy as np
    return np.fromfile(path, dtype='<f4', offset=44)  # tidbank_render writes a plain 44-byte header

def probe_judge(delays, heads, need_between=None):
    """-> (ok, text). heads: expected head ids, each needs >= 1 record. need_between=(v0, v1): >= 1 record with speed strictly between."""
    worst = max((abs(float(r['diff_samples'])) for r in delays), default=99.0)
    per = {h: sum(1 for r in delays if int(r['head']) == h) for h in heads}
    ok = worst <= 1.0 and all(c >= 1 for c in per.values()) and len(delays) > 0
    txt = f'records={len(delays)} per_head={per} max|measured-computed|={worst:.4f} smp'
    if need_between:
        lo, hi = sorted(need_between); mid = [r for r in delays if lo + 1e-3 < float(r['speed']) < hi - 1e-3]
        ok &= len(mid) >= 1; txt += f' during_glide={len(mid)}'
    return ok, txt

def echo_centroid_err(wav, rows, half=8):
    """Largest |energy centroid of the echo around n_out - n_out| over rows, in samples (solo head, echo-only render)."""
    y = read_f32_wav(wav).astype('float64'); worst = 0.0
    import numpy as np
    for r in rows:
        c = int(round(float(r['n_out']))); lo, hi = max(c - half, 0), min(c + half + 1, len(y))
        e = y[lo:hi] ** 2
        if e.sum() <= 1e-6: return 99.0
        worst = max(worst, abs(float((np.arange(lo, hi) * e).sum() / e.sum()) - float(r['n_out'])))
    return worst

def probe_tape_case(name, sched=None, between=None):
    base = ['--engine', 'tape', '--dry', '0', '--stim', 'clicks', '--bpm', '120', '--div', '0.5']
    def fn():
        sc = ['--sched', sched] if sched else []
        _, rows, _ = probe_run(name + '_all', base + ['--params', 'mode=7,intensity=0'] + sc)
        ok, txt = probe_judge(rows, (1, 2, 3), between)
        for h in (1, 2, 3):  # solo head: the WAV holds that head's echoes only, so the centroid is checkable
            wav, hr, _ = probe_run(f'{name}_h{h}', base + ['--params', f'head={h},intensity=0'] + sc)
            ok_h, _ = probe_judge(hr, (h,), between if h == 1 else None)
            ce = echo_centroid_err(wav, hr)
            ok &= ok_h and ce <= 1.0; txt += f' | h{h} wav_centroid_err={ce:.3f}'
        return ok, txt
    return Case(name, fn=fn, judge=None)

def probe_bbd_case(name, pid):
    def fn():
        _, rows, _ = probe_run(name, ['--engine', 'bbd', '--profile', pid, '--stim', 'clicks', '--bpm', '120', '--params', 'time_ms=300,fb=0,noise=0'])
        ok, txt = probe_judge(rows, (0,))
        want = 300.0 * 48.0
        off = max(abs(float(r['computed_samples']) - want) for r in rows) if rows else 99.0
        return ok and off <= 1.0, txt + f' computed vs 300 ms: {off:.4f} smp'
    return Case(name, fn=fn, judge=None)

def sha(path):
    import hashlib
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()[:16]

def probe_null_case(name, args):
    """Probe on (csv written) == probe-off renderer == probe renderer without the flag, byte for byte; plus the pre-change
    renderer when TIDBANK_BASELINE_RENDER points at one (a build of origin/bbd-plugin)."""
    def fn():
        os.makedirs(OUT, exist_ok=True)
        outs = {}
        def render(tag, exe, extra):
            w = os.path.join(OUT, f'{name}_{tag}.wav')
            p = subprocess.run([exe, '--out', w] + args + extra, capture_output=True, text=True)
            if p.returncode != 0: raise RuntimeError(f'{tag} rc={p.returncode} {p.stderr.strip()[-200:]}')
            outs[tag] = sha(w)
        render('off', RENDER, [])
        render('probe_binary_flag_unset', RENDER_PROBE, [])
        render('probe_on', RENDER_PROBE, ['--probe-csv', os.path.join(OUT, f'{name}_on.csv'), '--probe-interval', '0.5', '--probe-offset-s', '0.1'])
        base = os.environ.get('TIDBANK_BASELINE_RENDER')
        if base: render('baseline', base, [])
        ok = len(set(outs.values())) == 1
        return ok, ' '.join(f'{k}={v}' for k, v in outs.items()) + ('' if base else ' (set TIDBANK_BASELINE_RENDER for the pre-change comparison)')
    return Case(name, fn=fn, judge=None)

def probe_cases():
    long_ = ['--secs', '8']
    return [
        probe_tape_case('p_tape_const'),
        probe_tape_case('p_tape_glide', sched='2.2:bpm=100', between=(2.0, 1.0 / 0.6)),  # head 1: v = 0.5 / (0.5 * 60 / bpm)
        probe_bbd_case('p_bbd', 'm8192'),
        probe_null_case('p_null_tape', ['--engine', 'tape', '--stim', 'clicks', '--bpm', '120', '--div', '0.5', '--dry', '1', '--secs', '8',
                                        '--params', 'mode=7,intensity=0.7,drive=6', '--sched', '2.2:bpm=100;5.0:mode=4,spacing=1']),
        probe_null_case('p_null_bbd', ['--engine', 'bbd', '--profile', 'd4096', '--stim', 'noise', '--secs', '8', '--dry', '1',
                                       '--params', 'time_ms=200,fb=0.4', '--sched', '2.0:time_ms=120,bright=0']),
        probe_null_case('p_null_tape_44k', ['--engine', 'tape', '--stim', 'tone', '--sr', '44100', '--secs', '6', '--dry', '1',
                                            '--params', 'mode=6,intensity=0.5', '--sched', '2.0:nudge=50']),
    ]

CASES: List[Case] = [
    Case('selftest', script=SELFTEST, judge=selftest_judge),
    sync_case(90), sync_case(120), sync_case(128),
    Case('transparency_thd', render=TONE, analyze=['thd', '{wav}', '--f0', '1000', '--max-thd-db', '-100'],
         judge=ok_flag(['thd_db'])),
    Case('transparency_health', render=TONE, analyze=['health', '{wav}'], judge=ok_flag(['peak_dbfs', 'subnormals'])),
    *S2,
    *s3_cases(),
    *S4,
    *S4X,
    *S7,
    *bbd_cases(),
    *probe_cases(),
]

def run_case(c: Case):
    if c.fn:
        return c.fn()
    if c.script:
        p = subprocess.run([sys.executable, c.script], capture_output=True, text=True)
        lines = p.stdout.strip().splitlines()
        res = {'line': lines[-1] if lines else p.stderr.strip()[-200:]}
        ok, det = c.judge(res)
        return ok and p.returncode == 0, det
    os.makedirs(OUT, exist_ok=True)
    if c.multi:
        rs = []
        for i, (rn, an) in enumerate(c.multi):
            ok, r = render_analyze(c.name + f'_{i}', rn, an)
            if not ok: return False, r
            rs.append(r)
        return c.judge(rs)
    ok, r = render_analyze(c.name, c.render, c.analyze)
    return c.judge(r) if ok else (False, r)

def render_analyze(name, render, analyze):
    wav = os.path.join(OUT, name + '.wav')
    sub = lambda args: [wav if a == '{wav}' else a for a in args]
    t0 = time.time(); p = subprocess.run([RENDER] + sub(render), capture_output=True, text=True); t_render = time.time() - t0
    if p.returncode != 0:
        return False, f'render failed rc={p.returncode} {p.stderr.strip()}'
    p = subprocess.run([sys.executable, ANALYZE] + sub(analyze), capture_output=True, text=True)
    try:
        res = json.loads(p.stdout.strip().splitlines()[-1]); res['_render_s'] = t_render; return True, res
    except Exception:
        return False, f'analyzer gave no JSON rc={p.returncode} {p.stderr.strip()[-200:]}'

def main(argv):
    prefix = argv[1] if len(argv) > 1 else ''
    cases = [c for c in CASES if c.name.startswith(prefix)]
    if not cases:
        print(col('31', f'no case matches prefix "{prefix}"; have: ' + ', '.join(c.name for c in CASES))); return 2
    failed = []; known = []
    for c in cases:
        t0 = time.time()
        try: ok, det = run_case(c)
        except Exception as e: ok, det = False, f'exception {e!r}'
        if c.known:
            tag = col('33', 'KNOWN') if not ok else col('33', 'FIXED?')
            det = f'{det} [known: {c.known}]'
        else:
            tag = col('32', 'PASS') if ok else col('31', 'FAIL')
        print(f'{tag} {c.name:<24} {det} ({time.time() - t0:.1f}s)')
        if c.known and ok: failed.append(c.name + ' (FIXED? remove from KNOWN)')
        elif not ok and not c.known: failed.append(c.name)
        elif c.known: known.append(c.name)
    n = len(cases)
    summary = f'{n - len(failed) - len(known)}/{n} passed' + (f', {len(known)} known: {", ".join(known)}' if known else '') \
        + (f', FAILED: {", ".join(failed)}' if failed else '')
    print(col('31' if failed else '32', summary))
    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
