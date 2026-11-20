#!/usr/bin/env python3
"""Measure the Matriarch's filter SPACING from a stereo recording (protocol: docs/research/matriarch-spacing-measurement.md).

Both filters self-oscillate in LP/LP STEREO mode, so each output channel carries one sine: VCF 2 stays put, VCF 1 moves with
SPACING. The script finds the steady stretches (one per SPACING position), measures both frequencies, and prints the offset
of VCF 1 against VCF 2 in octaves. Which channel is VCF 1 is detected (the one that moves), so L/R order does not matter.

Run:  uv run --with numpy tools/matriarch/spacing.py measure take.wav [--min-s 1.5] [--json]
      uv run --with numpy tools/matriarch/spacing.py selftest
WAV: 16/24/32-bit PCM or 32-bit float, stereo (Live's default export works)."""
import sys, json, struct, argparse
import numpy as np


def read_wav(path):
    """All channels as float64 [n, ch] and the sample rate. Handles PCM 16/24/32, float32 and WAVE_FORMAT_EXTENSIBLE."""
    b = open(path, 'rb').read()
    if b[:4] != b'RIFF' or b[8:12] != b'WAVE': raise ValueError(f'{path}: not a WAV file')
    i, fmt = 12, None
    while i + 8 <= len(b):
        cid, sz = b[i:i+4], struct.unpack('<I', b[i+4:i+8])[0]; d = b[i+8:i+8+sz]
        if cid == b'fmt ':
            tag, ch, sr, _, _, bits = struct.unpack('<HHIIHH', d[:16])
            if tag == 0xFFFE: tag = struct.unpack('<H', d[24:26])[0]  # extensible: sub-format GUID starts with the tag
            fmt = (tag, ch, sr, bits)
        elif cid == b'data':
            if fmt is None: raise ValueError('data before fmt')
            tag, ch, sr, bits = fmt
            if tag == 3 and bits == 32: x = np.frombuffer(d, '<f4').astype(np.float64)
            elif tag == 3 and bits == 64: x = np.frombuffer(d, '<f8').copy()
            elif tag == 1 and bits == 16: x = np.frombuffer(d, '<i2') / 32768.0
            elif tag == 1 and bits == 32: x = np.frombuffer(d, '<i4') / 2147483648.0
            elif tag == 1 and bits == 24:
                r = np.frombuffer(d[:len(d) - len(d) % 3], np.uint8).reshape(-1, 3).astype(np.int32)
                x = ((r[:, 0] << 8 | r[:, 1] << 16 | r[:, 2] << 24) >> 8) / 8388608.0
            else: raise ValueError(f'unsupported WAV format tag {tag}, {bits} bits')
            n = len(x) // ch
            return x[:n * ch].reshape(n, ch), sr
        i += 8 + sz + (sz & 1)
    raise ValueError('no data chunk')


def peak_freq(frame, sr, nfft):
    """Frequency (Hz) and level (dBFS, sine peak) of the strongest spectral peak, with parabolic interpolation on log power."""
    w = np.hanning(len(frame))
    X = np.abs(np.fft.rfft(frame * w, nfft)); X[0] = 0.0
    k = int(np.argmax(X))
    if 0 < k < len(X) - 1:
        a, b, c = np.log(X[k-1] + 1e-30), np.log(X[k] + 1e-30), np.log(X[k+1] + 1e-30)
        den = a - 2 * b + c; p = 0.5 * (a - c) / den if den != 0 else 0.0
    else: p = 0.0
    amp = 2 * X[k] / w.sum()
    return (k + p) * sr / nfft, 20 * np.log10(amp + 1e-30)


def track(x, sr, frame_s=0.5, hop_s=0.1):
    """Per frame and channel: frequency and level. Returns times [m], f [m, ch], db [m, ch]."""
    n, hop = int(frame_s * sr), int(hop_s * sr)
    nfft = 1 << int(np.ceil(np.log2(n * 8)))  # zero-pad x8: sub-Hz bins before interpolation
    starts = range(0, max(1, len(x) - n + 1), hop)
    f = np.zeros((len(starts), x.shape[1])); db = np.zeros_like(f)
    for j, s in enumerate(starts):
        for c in range(x.shape[1]): f[j, c], db[j, c] = peak_freq(x[s:s+n, c], sr, nfft)
    return np.array([(s + n / 2) / sr for s in starts]), f, db


def segments(t, f, db, min_s, tol=0.003, floor_db=-60.0):
    """Steady stretches: consecutive frames whose frequencies (every channel) stay within tol (relative) of the stretch's first
    frame and whose levels are above floor_db. Keeps stretches of at least min_s seconds; trims one frame at each end."""
    out, j, m = [], 0, len(t)
    while j < m:
        k = j + 1
        while k < m and np.all(np.abs(f[k] / f[j] - 1) < tol) and np.all(db[k] > floor_db) and np.all(db[j] > floor_db): k += 1
        if k - j >= 3 and t[k-1] - t[j] >= min_s:
            a, z = j + 1, k - 1
            out.append(dict(start_s=round(float(t[a]), 2), end_s=round(float(t[z-1]), 2),
                            f=[float(np.median(f[a:z, c])) for c in range(f.shape[1])],
                            db=[float(np.median(db[a:z, c])) for c in range(f.shape[1])],
                            spread=[float(np.ptp(f[a:z, c]) / np.median(f[a:z, c])) for c in range(f.shape[1])]))
        j = k
    return out


def analyse(x, sr, min_s):
    if x.shape[1] < 2: raise ValueError('need a stereo recording: one channel per filter')
    t, f, db = track(x[:, :2], sr)
    segs = segments(t, f, db, min_s)
    if not segs: return dict(ok=False, error='no steady stretch found: are both filters self-oscillating, VCA MODE on DRONE?', segments=[])
    # VCF 2 is the channel whose frequency moves least across the stretches; VCF 1 the other one
    logf = np.log2(np.array([s['f'] for s in segs]))
    moving = np.ptp(logf, axis=0)
    vcf2 = int(np.argmin(moving)); vcf1 = 1 - vcf2
    ref = float(np.median([s['f'][vcf2] for s in segs]))
    rows = []
    for i, s in enumerate(segs):
        rows.append(dict(n=i + 1, start_s=s['start_s'], end_s=s['end_s'],
                         vcf1_hz=round(s['f'][vcf1], 2), vcf2_hz=round(s['f'][vcf2], 2),
                         octaves=round(float(np.log2(s['f'][vcf1] / s['f'][vcf2])), 4),
                         vcf1_db=round(s['db'][vcf1], 1), vcf2_db=round(s['db'][vcf2], 1),
                         vcf2_drift_cents=round(1200 * float(np.log2(s['f'][vcf2] / ref)), 1)))
    return dict(ok=True, sample_rate=sr, vcf1_channel='LR'[vcf1], vcf2_channel='LR'[vcf2],
                vcf2_moved_octaves=round(float(moving[vcf2]), 4), segments=rows)


def show(r):
    if not r['ok']: print('FAIL', r['error']); return
    print(f"VCF 1 = {r['vcf1_channel']} channel (moves), VCF 2 = {r['vcf2_channel']} channel "
          f"(moved {r['vcf2_moved_octaves'] * 1200:.0f} cents across the take; should be near 0)")
    print(f"{'n':>3} {'from s':>7} {'to s':>7} {'VCF1 Hz':>10} {'VCF2 Hz':>10} {'octaves':>8} {'VCF1 dB':>8} {'VCF2 drift c':>12}")
    for s in r['segments']:
        print(f"{s['n']:>3} {s['start_s']:>7.1f} {s['end_s']:>7.1f} {s['vcf1_hz']:>10.2f} {s['vcf2_hz']:>10.2f} "
              f"{s['octaves']:>+8.3f} {s['vcf1_db']:>8.1f} {s['vcf2_drift_cents']:>12.1f}")
    print('Match each row to the SPACING mark you set, in order. A missing mark means VCF 1 stopped oscillating there '
          '(out of range): note it as a limit.')


def selftest():
    """Synthetic take: VCF 2 fixed at 440 Hz on R, VCF 1 on L stepping through known offsets with 0.3 s glides between."""
    sr, hold, glide = 48000, 2.0, 0.3
    offs = [0.0, 0.5, 1.0, 2.0, 3.25, -1.0, -2.5, -4.0]
    fl, fr = [], []
    prev = offs[0]
    for o in offs:
        g = np.linspace(prev, o, int(glide * sr), endpoint=False); fl.append(440 * 2 ** g); fr.append(np.full(len(g), 440.0))
        fl.append(np.full(int(hold * sr), 440 * 2 ** o)); fr.append(np.full(int(hold * sr), 440.0)); prev = o
    fl, fr = np.concatenate(fl), np.concatenate(fr)
    rng = np.random.default_rng(1)
    L = 0.3 * np.sin(2 * np.pi * np.cumsum(fl) / sr) + 1e-4 * rng.standard_normal(len(fl))
    R = 0.25 * np.sin(2 * np.pi * np.cumsum(fr) / sr) + 1e-4 * rng.standard_normal(len(fr))
    r = analyse(np.stack([L, R], 1), sr, 1.0)
    got = [s['octaves'] for s in r['segments']]
    ok = r['ok'] and r['vcf1_channel'] == 'L' and len(got) == len(offs) and all(abs(a - b) < 0.002 for a, b in zip(got, offs))
    print(json.dumps(dict(selftest='pass' if ok else 'FAIL', want=offs, got=got)))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    m = sub.add_parser('measure'); m.add_argument('wav'); m.add_argument('--min-s', type=float, default=1.5); m.add_argument('--json', action='store_true')
    sub.add_parser('selftest')
    a = ap.parse_args()
    if a.cmd == 'selftest': return selftest()
    x, sr = read_wav(a.wav)
    r = analyse(x, sr, a.min_s)
    print(json.dumps(r, indent=1)) if a.json else show(r)
    return 0 if r['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
