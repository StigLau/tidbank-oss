#!/usr/bin/env python3
"""Measure the Matriarch's stereo delay through a 2-in/2-out interface (protocol: docs/research/matriarch-measurement.md, "Rig A").

The script plays a test signal on the interface's left output, records both inputs at the same time, saves the take
(played + both inputs) with its settings, and analyses it:
  clicks   short noise bursts -> delay time (ms) per burst, repeat levels (dB per pass)
  noise    longer pink-noise bursts -> wet/reference magnitude response, -3/-6 dB points, slope
  silence  nothing played -> noise floor and tonal lines (BBD clock whine)

Rig A, MIDI source (default): the Matriarch plays short notes sent to its USB MIDI port; MIX fully counterclockwise so
MAIN OUT L carries the dry VCA 1 signal (Delay 1's input) -> interface IN L (reference); rear DELAY OUT L (100 % wet)
-> interface IN R. For Delay 1 against Delay 2: DELAY OUT L -> IN L, DELAY OUT R -> IN R, --ch wet1,wet2.
--source audio instead plays the signal on interface OUT L (-> MULT -> INPUT 1, MULT -> IN L).

Run:  uv run --with numpy --with scipy --with sounddevice --with mido --with python-rtmidi tools/matriarch/delay.py <cmd>
      devices | probe | take NAME --signal clicks|noise|silence [...] | sweep NAME --cc time --values 0:16383:9
      | seq NAME --preset slew|ramp|wobble|echoes [...] | analyse WAV | selftest"""
import sys, os, json, time, struct, argparse, datetime
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spacing import read_wav  # noqa: E402

SR = 48000
CC = {'time': (12, 44), 'spacing': (13, 45)}  # Matriarch manual p. 74: MSB, LSB, 0-16383
DEFAULT_DIR = os.path.expanduser(f'~/Music/matriarch-measurements/{datetime.date.today()}/delay')


# ── signals ────────────────────────────────────────────────────────────────
def burst(n, seed):
    """Hann-windowed white noise burst: broadband, so its cross-correlation peak is sharp."""
    return np.random.default_rng(seed).standard_normal(n) * np.hanning(n)


def pink(n, seed):
    X = np.fft.rfft(np.random.default_rng(seed).standard_normal(n)); f = np.arange(len(X)); f[0] = 1
    x = np.fft.irfft(X / np.sqrt(f), n); return x / np.sqrt(np.mean(x ** 2))


def make_signal(kind, level_db, period, count, lead=1.0):
    """Returns (signal, event start times in s, event length in s)."""
    if kind == 'silence':
        return np.zeros(int((lead + period * count) * SR)), [], 0.0
    ev_len = 0.010 if kind == 'clicks' else 0.300
    ev = burst(int(ev_len * SR), 1) if kind == 'clicks' else pink(int(ev_len * SR), 2) * np.hanning(int(ev_len * SR)) ** 0.25
    ev = ev / np.max(np.abs(ev)) * 10 ** (level_db / 20)
    x = np.zeros(int((lead + period * count) * SR)); starts = [lead + k * period for k in range(count)]
    for s in starts: x[int(s * SR):int(s * SR) + len(ev)] = ev
    return x, starts, ev_len


# ── I/O ────────────────────────────────────────────────────────────────────
IN_CH = (0, 1)  # 0-based interface inputs used as IN L, IN R; set from --in-ch


def find_device(name):
    import sounddevice as sd
    for i, d in enumerate(sd.query_devices()):
        if name.lower() in d['name'].lower() and d['max_input_channels'] > max(IN_CH): return i
    raise SystemExit(f'no 2-in/2-out audio device matching "{name}"; see: delay.py devices')


def playrec(x, device):
    import sounddevice as sd
    out = np.zeros((len(x), 2), np.float32); out[:, 0] = x
    rec = sd.playrec(out, SR, channels=max(IN_CH) + 1, device=(device, device), dtype='float32'); sd.wait()
    return rec[:, list(IN_CH)].astype(np.float64)


def rec_midi(starts, gate, total_s, device, port, note=60, vel=110):
    """Record both inputs while the Matriarch plays one note per start time (its own sound is the test signal)."""
    import sounddevice as sd, mido
    rec = sd.rec(int(total_s * SR), SR, channels=max(IN_CH) + 1, device=device, dtype='float32'); t0 = time.monotonic()
    with mido.open_output(port) as o:
        for s in starts:
            time.sleep(max(0.0, t0 + s - time.monotonic())); o.send(mido.Message('note_on', note=note, velocity=vel))
            time.sleep(gate); o.send(mido.Message('note_off', note=note))
    sd.wait(); return rec[:, list(IN_CH)].astype(np.float64)


def send_cc(port, which, value, lsb=False):
    """MSB only by default. Bench 2026-11-17 (Model 24 rig, TIME): MSB alone is clean (0 -> 43.6 ms with noise, 45.5 ms
    from a square note; 64 -> 224.2 ms, 127 -> 403.6 ms); with CC44 sent too, a 17-step sweep stayed at 222.8/225.6 ms,
    and a raised LSB shortened the delay."""
    import mido
    msb_cc, lsb_cc = CC[which]; value = int(np.clip(value, 0, 16383))
    with mido.open_output(port) as o:
        o.send(mido.Message('control_change', control=msb_cc, value=value >> 7))
        if lsb: o.send(mido.Message('control_change', control=lsb_cc, value=value & 127))


def write_wav(path, x):
    """32-bit float WAV, x is [n, ch]."""
    x = np.asarray(x, '<f4'); n, ch = x.shape
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + x.nbytes) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 3, ch, SR, SR * ch * 4, ch * 4, 32))
        f.write(b'data' + struct.pack('<I', x.nbytes) + x.tobytes())


# ── analysis ───────────────────────────────────────────────────────────────
def xcorr(a, t):
    n = 1 << int(np.ceil(np.log2(len(a) + len(t))))
    return np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(t, n)), n)[:len(a)]


def interp_peak(c, k):
    if 0 < k < len(c) - 1:
        a, b, g = c[k - 1], c[k], c[k + 1]; d = a - 2 * b + g
        return k + (0.5 * (a - g) / d if d else 0.0)
    return float(k)


def lag(ref, x, max_s):
    """Lag (samples, fractional) of x behind ref, from the cross-correlation peak within 0..max_s."""
    c = np.abs(xcorr(x, ref[:min(len(ref), len(x))]))[:int(max_s * SR)]
    return interp_peak(c, int(np.argmax(c)))


def roles(meta, rec):
    """Map channel roles to signals: 'ref' (falls back to the played signal), 'wet1', 'wet2'."""
    r = {name: rec[:, i] for i, name in enumerate(meta['ch'])}
    if 'ref' not in r: r['ref'] = meta['_played']; r['_ref_is_played'] = True
    return r


def event_starts(meta, r):
    """Event times. With a recorded reference (MIDI source) they are found in it: the audio stream starts an unknown
    100-200 ms after the script's clock, so the scheduled note times do not locate the notes in the recording."""
    if r.get('_ref_is_played') or meta.get('source') != 'midi': return list(meta['starts'])
    y = r['ref']; h = SR // 100; w = y[:len(y) // h * h].reshape(-1, h)
    db = 20 * np.log10(np.sqrt((w ** 2).mean(1)) + 1e-30); th = db.max() - 20; gap = 0.3 * meta['period'] * 100
    on = []
    for i in range(1, len(db)):
        if db[i] > th and db[i - 1] <= th and (not on or i - on[-1] > gap): on.append(i)
    return [i / 100 for i in on]


def an_clicks(meta, rec):
    r = roles(meta, rec); ref = r['ref']; period = meta['period']
    tmpl = burst(int(0.010 * SR), 1)
    lat = lag(meta['_played'], ref, 0.2) / SR if not r.get('_ref_is_played') and meta.get('source') == 'audio' else 0.0
    out = {'reference': 'played signal (includes interface latency)' if r.get('_ref_is_played') else 'IN via mult',
           'ref_latency_ms': round(lat * 1000, 3), 'wet': {}}
    for w in [k for k in ('wet1', 'wet2') if k in r]:
        bursts = []
        for s in event_starts(meta, r):
            a = int(max(0.0, s - 0.05 + lat) * SR); refseg = ref[a:a + int((period - 0.1) * SR)]
            if r.get('_ref_is_played'): k0, t = int(0.05 * SR), tmpl
            else:  # the reference is a real input (MIDI source): the template is the dry note itself
                env = np.abs(refseg)
                if env.max() < 1e-6: bursts.append(None); continue
                k0 = max(0, int(np.argmax(env > 0.3 * env.max())) - 48); t = refseg[k0:k0 + int(0.03 * SR)]
            seg = r[w][a + k0:a + len(refseg)]
            cs = xcorr(seg, t)
            if np.abs(cs).max() < 1e-9: bursts.append(None); continue
            # coarse t_d: cross-correlation of 0.25 ms envelopes (first 30 ms of the note against the wet). A steady periodic
            # note's waveform matches at every period, and with |c| at every half period with inverted sign (3.75 ms off
            # at 130 Hz on the bench), so the waveform only refines within 1.5 ms of the coarse value.
            h = SR // 4000
            ev = lambda y: np.sqrt((y[:len(y) // h * h].reshape(-1, h) ** 2).mean(1))
            ew, et = ev(seg), ev(t); ce = xcorr(ew - ew.mean(), et - et.mean())
            kc = int(np.argmax(ce)) * h
            bursts.append(dict(seg=seg, cs=cs, kc=kc, k0=k0, refseg=refseg))
        # polarity of the delay path, once per take: sign of the signed correlation at the |c| peak, by majority
        signs = []
        for b in bursts:
            if b:
                lo = max(0, b['kc'] - 72); hi = min(len(b['cs']), b['kc'] + 72); k = lo + int(np.argmax(np.abs(b['cs'][lo:hi])))
                signs.append(np.sign(b['cs'][k]))
        pol = 1.0 if not signs or np.sum(signs) >= 0 else -1.0
        out.setdefault('polarity', {})[w] = 'inverted' if pol < 0 else 'same'
        rows = []
        for b in bursts:
            if b is None: rows.append(None); continue
            seg, c, kc, k0, refseg = b['seg'], pol * b['cs'], b['kc'], b['k0'], b['refseg']
            lo = max(0, kc - 72); hi = min(len(c), kc + 72)
            k1 = lo + int(np.argmax(c[lo:hi]))
            td = interp_peak(c, k1) / SR
            win = lambda k: np.sqrt(np.mean(seg[max(0, k - 96):k + 576] ** 2)) + 1e-30
            reps = []  # pass m sits at m * t_d: take the correlation maximum within 3 ms of it, stop when it is gone
            for m in (range(2, 6) if k1 > 0.005 * SR else ()):  # t_d near 0: wet is dry (patch), no repeats to find
                lo, hi = m * k1 - 144, m * k1 + 144
                if hi + 576 > len(seg): break
                k = lo + int(np.argmax(c[lo:hi]))
                if c[k] < c[k1] * 10 ** (-40 / 20): break
                reps.append(k)
            ref_rms = np.sqrt(np.mean(refseg[k0:k0 + 672] ** 2)) + 1e-30
            rows.append(dict(td_ms=round(td * 1000, 3), level_db=round(20 * np.log10(win(k1) / ref_rms), 2),
                             repeats_db=[round(20 * np.log10(win(k) / win(k1)), 2) for k in reps[:4]]))
        ok = [x for x in rows if x]
        out['wet'][w] = dict(per_event=rows, td_ms_median=round(float(np.median([x['td_ms'] for x in ok])), 3) if ok else None,
                             td_ms_spread=round(float(np.ptp([x['td_ms'] for x in ok])), 3) if ok else None)
    if all(k in out['wet'] and out['wet'][k]['td_ms_median'] for k in ('wet1', 'wet2')):
        out['wet1_minus_wet2_ms'] = round(out['wet']['wet1']['td_ms_median'] - out['wet']['wet2']['td_ms_median'], 3)
    return out


def an_pair(meta, rec):
    """Delay 1 against Delay 2, no dry reference (MIX fully cw: MAIN OUT L = Delay 1, R = Delay 2; or the DELAY OUTs).
    Per note: lag of wet1 behind wet2, which is negative when SPACING shortens Delay 1. Coarse from 0.25 ms envelopes
    over +-0.9 s, refined by the signed waveform correlation within 1.5 ms."""
    w1, w2 = rec[:, meta['ch'].index('wet1')], rec[:, meta['ch'].index('wet2')]
    y = np.abs(w1) + np.abs(w2); h = SR // 100; e = np.sqrt((y[:len(y) // h * h].reshape(-1, h) ** 2).mean(1))
    th = e.max() * 0.1; gap = 0.6 * meta['period'] * 100; on = []
    for i in range(1, len(e)):
        if e[i] > th and e[i - 1] <= th and (not on or i - on[-1] > gap): on.append(i)
    W, hh = int(0.9 * SR), SR // 4000
    ev = lambda v: np.sqrt((v[:len(v) // hh * hh].reshape(-1, hh) ** 2).mean(1))
    rows = []
    for i in on:
        s0 = i * h; k2 = s0 + int(np.argmax(np.abs(w2[s0:s0 + W]) > np.abs(w2[s0:s0 + W]).max() * 0.3)) - 48
        if k2 - W < 0 or k2 + W + int(0.05 * SR) > len(w1): rows.append(None); continue
        t = w2[k2:k2 + int(0.03 * SR)]; seg = w1[k2 - W:k2 + W + int(0.05 * SR)]
        ce = xcorr(ev(seg) - ev(seg).mean(), ev(t) - ev(t).mean()); kc = int(np.argmax(ce)) * hh
        cs = xcorr(seg, t); lo, hi = max(0, kc - 72), min(len(cs), kc + 72)
        sg = 1.0 if cs[lo + int(np.argmax(np.abs(cs[lo:hi])))] >= 0 else -1.0
        k = lo + int(np.argmax(sg * cs[lo:hi]))
        rows.append(dict(td_ms=round((interp_peak(sg * cs, k) - W) / SR * 1000, 3)))
    ok = [x['td_ms'] for x in rows if x]
    return {'reference': 'Delay 2 (wet2): values are t_d1 - t_d2', 'ref_latency_ms': 0.0,
            'wet': {'wet1': dict(per_event=[dict(td_ms=x['td_ms'], level_db=0.0, repeats_db=[]) if x else None for x in rows],
                                 td_ms_median=round(float(np.median(ok)), 3) if ok else None,
                                 td_ms_spread=round(float(np.ptp(ok)), 3) if ok else None)}}


def an_noise(meta, rec):
    from scipy.signal import welch
    r = roles(meta, rec); ref = r['ref']; L = int(meta['ev_len'] * SR); out = {'wet': {}}
    for w in [k for k in ('wet1', 'wet2') if k in r]:
        Pr, Pw = 0, 0
        for s in event_starts(meta, r):
            a = int(max(0.0, s - 0.02) * SR); d = int(lag(ref[a:a + L], r[w][a:a + int(1.2 * SR)], 1.1))
            f, pr = welch(ref[a:a + L], SR, nperseg=4096); _, pw = welch(r[w][a + d:a + d + L], SR, nperseg=4096)
            Pr, Pw = Pr + pr, Pw + pw
        H = 10 * np.log10((Pw + 1e-30) / (Pr + 1e-30))
        bands = 2 ** np.arange(np.log2(40), np.log2(20000), 1 / 6)
        bands = np.array([b for b in bands if np.any((f >= b / 2 ** (1 / 12)) & (f < b * 2 ** (1 / 12)))])
        Hb = np.array([np.median(H[(f >= b / 2 ** (1 / 12)) & (f < b * 2 ** (1 / 12))]) for b in bands])
        Hb -= np.median(Hb[(bands >= 200) & (bands <= 800)])
        def cross(db):
            i = next((i for i in range(len(bands)) if bands[i] > 800 and Hb[i] < db), None)
            if i is None or i == 0: return None
            return float(bands[i - 1] * (bands[i] / bands[i - 1]) ** ((db - Hb[i - 1]) / (Hb[i] - Hb[i - 1])))
        f3, f6 = cross(-3), cross(-6)
        slope = None
        if f3:
            i1, i2 = np.argmin(abs(bands - f3)), np.argmin(abs(bands - 2 * f3))
            if i2 < len(bands) and Hb[i2] > -60: slope = round(float(Hb[i2] - Hb[i1]), 1)
        out['wet'][w] = dict(f_minus3_hz=round(f3) if f3 else None, f_minus6_hz=round(f6) if f6 else None,
                             slope_db_per_oct_above_f3=slope,
                             response_db={f'{b:.0f}': round(float(h), 1) for b, h in zip(bands, Hb)})
    return out


def an_silence(meta, rec):
    from scipy.signal import welch, medfilt
    r = roles(meta, rec); out = {'wet': {}}
    for w in [k for k in ('wet1', 'wet2') if k in r]:
        f, P = welch(r[w], SR, nperseg=1 << 15); db = 10 * np.log10(P + 1e-30)
        floor = medfilt(db, 301); ex = db - floor
        pk = [k for k in range(1, len(f) - 1) if 200 < f[k] < 23500 and ex[k] > 12 and db[k] >= db[k - 1] and db[k] > db[k + 1]]
        pk = sorted(pk, key=lambda k: -ex[k])[:8]
        band = (f >= 250) & (f <= 20000)
        rms = 10 * np.log10(np.sum(P[band]) * (f[1] - f[0]) + 1e-30)
        out['wet'][w] = dict(noise_dbfs_250_20k=round(float(rms), 1),
                             lines=[dict(hz=round(float(f[k]), 1), above_floor_db=round(float(ex[k]), 1)) for k in sorted(pk)])
    return out


def analyse(meta, rec):
    if meta['signal'] == 'clicks' and 'ref' not in meta['ch'] and {'wet1', 'wet2'} <= set(meta['ch']): return an_pair(meta, rec)
    return {'clicks': an_clicks, 'noise': an_noise, 'silence': an_silence}[meta['signal']](meta, rec)


def load_take(wav):
    x, sr = read_wav(wav)
    if sr != SR: raise SystemExit(f'{wav}: {sr} Hz, expected {SR}')
    meta = json.load(open(os.path.splitext(wav)[0] + '.json')); meta['_played'] = x[:, 0]  # zeros for --source midi
    return meta, x[:, 1:3]


def show(name, meta, res):
    print(f"── {name}  signal={meta['signal']}  ch={','.join(meta['ch'])}  note={meta.get('note', '')}  cc={meta.get('cc')}")
    if meta['signal'] == 'clicks':
        print(f"   reference: {res['reference']}, latency {res['ref_latency_ms']} ms")
        for w, v in res['wet'].items():
            print(f"   {w}: delay {v['td_ms_median']} ms (spread {v['td_ms_spread']} ms over {len(v['per_event'])} bursts)")
            for i, e in enumerate(v['per_event']):
                if e: print(f"      burst {i + 1}: {e['td_ms']:9.3f} ms  level {e['level_db']:+6.1f} dB  repeats {e['repeats_db']}")
        if 'wet1_minus_wet2_ms' in res: print(f"   Delay 1 - Delay 2: {res['wet1_minus_wet2_ms']} ms")
    elif meta['signal'] == 'noise':
        for w, v in res['wet'].items():
            print(f"   {w}: -3 dB {v['f_minus3_hz']} Hz, -6 dB {v['f_minus6_hz']} Hz, slope f3->2f3 {v['slope_db_per_oct_above_f3']} dB")
    else:
        for w, v in res['wet'].items():
            print(f"   {w}: noise {v['noise_dbfs_250_20k']} dBFS (250 Hz-20 kHz); lines: " +
                  ', '.join(f"{l['hz']} Hz (+{l['above_floor_db']} dB)" for l in v['lines']))


def do_take(a, name, cc=None):
    os.makedirs(a.dir, exist_ok=True)
    if cc:
        if not a.midi: raise SystemExit('--cc needs --midi PORT (see: delay.py devices)')
        send_cc(a.midi, *cc); time.sleep(a.settle)
    x, starts, ev_len = make_signal(a.signal, a.level, a.period, a.count)
    if a.source == 'midi':
        if 'ref' not in a.ch.split(',') and a.signal == 'noise': raise SystemExit('--signal noise needs a ref channel (dry main out)')
        rec = rec_midi(starts, ev_len if a.signal == 'noise' else a.gate, len(x) / SR, find_device(a.device), a.midi, a.note_num)
        x = np.zeros(len(rec))
    else: rec = playrec(x, find_device(a.device))
    base = os.path.join(a.dir, name)
    write_wav(base + '.wav', np.column_stack([x, rec[:, 0], rec[:, 1]]))
    meta = dict(signal=a.signal, level_db=a.level, period=a.period, count=a.count, starts=starts, ev_len=ev_len,
                ch=a.ch.split(','), note=a.note, source=a.source, midi_note=a.note_num, cc=dict(param=cc[0], value=cc[1]) if cc else None,
                device=a.device, in_ch=a.in_ch, recorded=datetime.datetime.now().isoformat(timespec='seconds'))
    json.dump(meta, open(base + '.json', 'w'), indent=1)
    meta['_played'] = x; res = analyse(meta, rec)
    json.dump(res, open(base + '.result.json', 'w'), indent=1)
    show(name, meta, res); return res


# ── TIME under change: note trains with CC12 moves (bench 2026-11-17) ───────
def seq_events(a):
    """(time s, kind, value) for a preset. Times are on the script clock; the recording starts within 0-200 ms of it."""
    if a.preset == 'slew':  # note train, CC12 jumps lo -> hi -> lo; measures how fast t_d settles
        ev = [(1.0 + k * 0.9, 'note', 0.03) for k in range(40)] + [(1.0 + 9.5 * 0.9, 'cc', a.hi), (1.0 + 24.5 * 0.9, 'cc', a.lo)]
        return ev, 1.0 + 0.9 * 40 + 1.0, a.lo
    if a.preset == 'ramp':  # one note, CC12 lo -> hi over --secs, a new note, back down: pitch of the repeats
        n = int(a.secs / 0.02)
        ev = [(1.0, 'note', 0.15), (6.4, 'note', 0.15)]
        ev += [(2.0 + i * 0.02, 'cc', round(a.lo + (a.hi - a.lo) * i / n)) for i in range(n + 1)]
        ev += [(7.0 + i * 0.02, 'cc', round(a.hi - (a.hi - a.lo) * i / n)) for i in range(n + 1)]
        return ev, 11.0, a.lo
    if a.preset == 'wobble':  # one note, CC12 sine between lo and hi at --rate for --secs, then centre: net pitch shift
        c, d, dt = (a.lo + a.hi) / 2, (a.hi - a.lo) / 2, min(0.025, 1 / (8 * a.rate))
        ev = [(1.0, 'note', 0.15), (2.0 + a.secs, 'cc', round(c))]
        ev += [(2.0 + i * dt, 'cc', round(c + d * np.sin(2 * np.pi * a.rate * i * dt))) for i in range(int(a.secs / dt))]
        return ev, 2.0 + a.secs + 9.0, round(c)
    if a.preset == 'echoes':  # four notes, no CC moves: which channel each echo lands on (ping-pong)
        return [(1.0 + k * 3.0, 'note', 0.03) for k in range(4)], 13.0, a.lo
    raise SystemExit(f'unknown preset {a.preset}')


def run_seq(ev, total_s, start_cc, a):
    import sounddevice as sd, mido
    dev = find_device(a.device)
    with mido.open_output(a.midi) as o:
        o.send(mido.Message('control_change', control=12, value=int(start_cc)))
        if a.pingpong is not None: o.send(mido.Message('control_change', control=88, value=127 if a.pingpong == 'on' else 0))
        time.sleep(1.5)
        rec = sd.rec(int(total_s * SR), SR, channels=max(IN_CH) + 1, device=dev, dtype='float32'); t0 = time.monotonic()
        for t, kind, v in sorted(ev, key=lambda e: e[0]):
            time.sleep(max(0.0, t0 + t - time.monotonic()))
            if kind == 'note':
                o.send(mido.Message('note_on', note=a.note_num, velocity=110)); time.sleep(v); o.send(mido.Message('note_off', note=a.note_num))
            else: o.send(mido.Message('control_change', control=12, value=int(np.clip(v, 0, 127))))
        sd.wait()
    return rec[:, list(IN_CH)].astype(np.float64)


def pitch_track(y, hop_s=0.1, fmin=40, fmax=1000):
    """(t, Hz, dB) per hop: strongest peak in fmin-fmax, 85 ms Hann frame, interpolated. Coarse: it cannot follow a
    pitch that moves within a frame, and near runaway feedback it may pick a low tone that is not the note."""
    n = 4096; hop = int(hop_s * SR); f = np.fft.rfftfreq(n * 4, 1 / SR); sel = (f > fmin) & (f < fmax); rows = []
    for s in range(0, len(y) - n, hop):
        X = np.abs(np.fft.rfft(y[s:s + n] * np.hanning(n), n * 4)); k = int(np.argmax(np.where(sel, X, 0)))
        a_, b_, c_ = np.log(X[k - 1:k + 2] + 1e-30); p = 0.5 * (a_ - c_) / (a_ - 2 * b_ + c_)
        rows.append((round((s + n / 2) / SR, 2), round((k + p) * SR / (n * 4), 2), round(20 * np.log10(X[k] / (n / 4) + 1e-30), 1)))
    return rows


def echo_events(y, top, th_db=-45, gap_ms=30):
    """(t, dB) where the 1 ms envelope crosses th_db below top; levels are 30 ms peaks, a few dB coarse."""
    h = SR // 1000; e = 20 * np.log10(np.sqrt((y[:len(y) // h * h].reshape(-1, h) ** 2).mean(1)) / top + 1e-30); ev = []
    for i in range(1, len(e)):
        if e[i] > th_db and e[i - 1] <= th_db and (not ev or i - ev[-1][0] > gap_ms): ev.append([i, float(e[i:i + 30].max())])
    return [(i / 1000, round(d, 1)) for i, d in ev]


def do_seq(a):
    ev, total, start = seq_events(a); rec = run_seq(ev, total, start, a)
    os.makedirs(a.dir, exist_ok=True); base = os.path.join(a.dir, a.name)
    write_wav(base + '.wav', rec)  # plain multichannel, IN order as --in-ch, for listening
    meta = dict(preset=a.preset, lo=a.lo, hi=a.hi, rate=a.rate, secs=a.secs, pingpong=a.pingpong, ch=a.ch.split(','),
                in_ch=a.in_ch, note=a.note, events=[list(e) for e in ev], recorded=datetime.datetime.now().isoformat(timespec='seconds'))
    json.dump(meta, open(base + '.json', 'w'), indent=1)
    r = {name: rec[:, i] for i, name in enumerate(meta['ch'])}
    if a.preset == 'slew':
        m = dict(signal='clicks', source='midi', period=0.9, starts=[e[0] for e in ev if e[1] == 'note'], ev_len=0.01,
                 ch=meta['ch'], _played=np.zeros(len(rec)))
        res = an_clicks(m, rec); per = res['wet']['wet1']['per_event']
        print(' '.join(f"{i + 1}:{e['td_ms'] if e else None}" for i, e in enumerate(per)))
    elif a.preset == 'echoes':
        top = max(np.abs(v).max() for v in r.values()); res = {k: echo_events(v, top) for k, v in r.items() if k != 'x'}
        for k, v in res.items(): print(k, ' '.join(f'{t:.3f}s/{d:.0f}dB' for t, d in v[:24]))
    else:
        res = {k: pitch_track(v) for k, v in r.items() if k != 'x'}; keys = list(res)
        print('  t s  ' + '  '.join(f'{k:>6} Hz    dB' for k in keys))
        for i in range(len(res[keys[0]])):
            print(f'{res[keys[0]][i][0]:5.1f} ' + '  '.join(f'{res[k][i][1]:9.1f} {res[k][i][2]:6.1f}' for k in keys))
    json.dump(res, open(base + '.result.json', 'w'), indent=1)


# ── self-test ──────────────────────────────────────────────────────────────
def selftest():
    from scipy.signal import butter, sosfilt
    ok = True
    x, starts, ev_len = make_signal('clicks', -12, 2.0, 3)
    tt = np.arange(int(0.2 * SR)) / SR; note = 0.3 * (2 * ((220 * tt) % 1) - 1) * np.exp(-tt / 0.02)
    x = np.zeros_like(x)
    for s0 in starts: x[int((s0 + 0.007) * SR):int((s0 + 0.007) * SR) + len(note)] += note  # MIDI jitter: 7 ms late
    lat, td = int(0.0043 * SR), 0.4
    lp = butter(4, 2600, fs=SR, output='sos')
    ref = np.roll(x, lat); wet = np.zeros_like(x)
    for k, g in [(1, 0.5), (2, 0.25), (3, 0.125)]: wet += g * np.roll(sosfilt(lp, ref), int(k * td * SR))
    meta = dict(signal='clicks', period=2.0, starts=starts, ev_len=ev_len, ch=['ref', 'wet1'], _played=x)
    r = an_clicks(meta, np.column_stack([ref, wet]))['wet']['wet1']
    e = r['per_event'][0]
    c1 = abs(r['td_ms_median'] - 400) < 0.5 and e['repeats_db'] and abs(e['repeats_db'][0] + 6.02) < 1.0
    print(f"clicks: delay {r['td_ms_median']} ms (want 400), repeats {e['repeats_db']} (want about -6, -12) -> {'pass' if c1 else 'FAIL'}")
    sq = np.zeros(int(6.5 * SR)); tt = np.arange(int(0.16 * SR)) / SR; note = 0.25 * np.sign(np.sin(2 * np.pi * 261.6 * tt))
    for s0 in (0.83, 2.83, 4.83): sq[int(s0 * SR):int(s0 * SR) + len(note)] = note
    wsq = np.roll(sosfilt(lp, sq), int(0.1234 * SR)) * 0.7
    meta = dict(signal='clicks', source='midi', period=2.0, starts=[1.0, 3.0, 5.0], ev_len=0.01, ch=['ref', 'wet1'],
                _played=np.zeros(len(sq)))
    r2 = an_clicks(meta, np.column_stack([sq, wsq]))['wet']['wet1']
    c1b = r2['td_ms_median'] is not None and abs(r2['td_ms_median'] - 123.4) < 0.5
    print(f"square note, binary envelope, notes 170 ms early: delay {r2['td_ms_median']} ms (want 123.4) -> {'pass' if c1b else 'FAIL'}")
    c1 = c1 and c1b
    nz = np.zeros(int(6.5 * SR)); rng = np.random.default_rng(5)
    for s0 in (1.1, 3.1, 5.1): nz[int(s0 * SR):int(s0 * SR) + int(0.03 * SR)] = rng.standard_normal(int(0.03 * SR)) * 0.2
    w2 = np.roll(sosfilt(lp, nz), int(0.2 * SR)); w1 = np.roll(sosfilt(lp, nz), int(0.1531 * SR))
    v = an_pair(dict(signal='clicks', period=2.0, ch=['wet1', 'wet2']), np.column_stack([w1, w2]))['wet']['wet1']
    c4 = v['td_ms_median'] is not None and abs(v['td_ms_median'] + 46.9) < 0.1
    print(f"pair: t_d1 - t_d2 {v['td_ms_median']} ms (want -46.9) -> {'pass' if c4 else 'FAIL'}"); c1 = c1 and c4
    x, starts, ev_len = make_signal('noise', -12, 2.0, 3)
    wet = np.roll(sosfilt(butter(6, 2600, fs=SR, output='sos'), x), int(0.4 * SR))
    meta = dict(signal='noise', period=2.0, starts=starts, ev_len=ev_len, ch=['ref', 'wet1'], _played=x)
    v = an_noise(meta, np.column_stack([x, wet]))['wet']['wet1']
    c2 = v['f_minus3_hz'] and abs(v['f_minus3_hz'] / 2600 - 1) < 0.1
    print(f"noise: -3 dB {v['f_minus3_hz']} Hz (want 2600), slope {v['slope_db_per_oct_above_f3']} dB/oct (6th order) -> {'pass' if c2 else 'FAIL'}")
    t = np.arange(10 * SR) / SR
    w = 1e-4 * np.random.default_rng(3).standard_normal(len(t)) + 1e-3 * np.sin(2 * np.pi * 5250 * t)
    meta = dict(signal='silence', ch=['ref', 'wet1'], _played=np.zeros(len(t)))
    v = an_silence(meta, np.column_stack([np.zeros(len(t)), w]))['wet']['wet1']
    c3 = any(abs(l['hz'] - 5250) < 5 for l in v['lines'])
    print(f"silence: lines {[l['hz'] for l in v['lines']]} (want 5250) -> {'pass' if c3 else 'FAIL'}")
    ok = c1 and c2 and c3
    print('selftest', 'pass' if ok else 'FAIL'); return 0 if ok else 1


# ── CLI ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('devices'); sub.add_parser('selftest')
    p = sub.add_parser('probe'); p.add_argument('--device', default='Digitakt')
    for name in ('take', 'sweep'):
        t = sub.add_parser(name); t.add_argument('name')
        t.add_argument('--signal', choices=['clicks', 'noise', 'silence'], default='clicks')
        t.add_argument('--level', type=float, default=-12.0, help='peak level of the played signal, dBFS')
        t.add_argument('--period', type=float, default=2.0, help='seconds between bursts (longer than the delay)')
        t.add_argument('--count', type=int, default=4)
        t.add_argument('--ch', default='ref,wet1', help='roles of IN L,IN R: ref|wet1|wet2|x')
        t.add_argument('--note', default='', help='panel settings for this take, e.g. "TIME 12:00, FEEDBACK min, Bright"')
        t.add_argument('--device', default='Model 24'); t.add_argument('--midi', default='Moog Matriarch')
        t.add_argument('--in-ch', default='13,14', help='interface inputs (1-based) recorded as IN L,IN R')
        t.add_argument('--source', choices=['midi', 'audio'], default='midi',
                       help='midi: the Matriarch plays notes (IN L = dry main out, MIX ccw); audio: OUT L plays the signal')
        t.add_argument('--note-num', type=int, default=60); t.add_argument('--gate', type=float, default=0.03, help='note length for clicks, s')
        t.add_argument('--dir', default=DEFAULT_DIR); t.add_argument('--settle', type=float, default=1.0)
        if name == 'take': t.add_argument('--cc', default=None, help='e.g. time=8192 or spacing=8192')
        else: t.add_argument('--cc', required=True, choices=list(CC)); t.add_argument('--values', required=True, help='start:stop:count')
    q = sub.add_parser('seq', help='TIME under change: slew | ramp | wobble | echoes (see seq_events)'); q.add_argument('name')
    q.add_argument('--preset', required=True, choices=['slew', 'ramp', 'wobble', 'echoes'])
    q.add_argument('--lo', type=int, default=40, help='CC12 MSB'); q.add_argument('--hi', type=int, default=80, help='CC12 MSB')
    q.add_argument('--rate', type=float, default=2.0, help='wobble Hz'); q.add_argument('--secs', type=float, default=3.0, help='wobble/ramp length')
    q.add_argument('--pingpong', choices=['on', 'off'], default=None, help='send CC88 first')
    q.add_argument('--ch', default='L,R', help='names of the recorded inputs; slew needs ref,wet1')
    q.add_argument('--in-ch', default='13,14'); q.add_argument('--note', default=''); q.add_argument('--note-num', type=int, default=60)
    q.add_argument('--device', default='Model 24'); q.add_argument('--midi', default='Moog Matriarch'); q.add_argument('--dir', default=DEFAULT_DIR)
    an = sub.add_parser('analyse'); an.add_argument('wav')
    a = ap.parse_args()
    if a.cmd == 'selftest': return selftest()
    if a.cmd == 'devices':
        import sounddevice as sd; print(sd.query_devices())
        try:
            import mido; print('MIDI out:', mido.get_output_names())
        except ImportError: print('MIDI: mido not installed')
        return 0
    if a.cmd == 'probe':
        dev = find_device(a.device); t = np.arange(int(1.5 * SR)) / SR
        x = np.where((t > 0.25) & (t < 1.25), 10 ** (-30 / 20) * np.sin(2 * np.pi * 1000 * t), 0.0)
        rec = playrec(x, dev)
        for c in range(2):
            y = rec[int(0.4 * SR):int(1.1 * SR), c]; rms = 20 * np.log10(np.sqrt(np.mean(y ** 2)) + 1e-30)
            print(f"IN {'LR'[c]}: {rms:6.1f} dBFS rms" + (f", {lag(x, rec[:, c], 0.3) / SR * 1000:.2f} ms behind OUT L" if rms > -70 else ' (no signal)'))
        return 0
    if a.cmd == 'analyse':
        meta, rec = load_take(a.wav); res = analyse(meta, rec); show(os.path.basename(a.wav), meta, res); return 0
    if hasattr(a, 'in_ch'):
        global IN_CH; IN_CH = tuple(int(c) - 1 for c in a.in_ch.split(','))
    if a.cmd == 'seq': do_seq(a); return 0
    if a.cmd == 'take':
        cc = None
        if a.cc: k, v = a.cc.split('='); cc = (k, int(v))
        do_take(a, a.name, cc); return 0
    lo, hi, n = a.values.split(':'); vals = np.linspace(int(lo), int(hi), int(n)).round().astype(int)
    summary = []
    for v in vals:
        res = do_take(a, f'{a.name}-{a.cc}{v:05d}', (a.cc, int(v)))
        if a.signal == 'clicks': summary.append((v, {w: x['td_ms_median'] for w, x in res['wet'].items()}))
    for v, d in summary: print(f'{a.cc} {v:5d}: ' + '  '.join(f'{w} {ms} ms' for w, ms in d.items()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
