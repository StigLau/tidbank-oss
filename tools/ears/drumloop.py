#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy"]
# ///
"""Deterministic synthetic drum loop (kick, snare, closed hat) at an exact tempo, float32 mono WAV.
For the S8 listening gate: bars of groove, then bars of silence so the echo tails are heard alone.
Usage: uv run --locked --script tools/ears/drumloop.py out.wav --bpm 120 [--bars 4] [--tail-bars 2] [--sr 48000]"""
import argparse
import struct
import numpy as np

def write_wav(path, x, sr):  # float32 mono
    d = np.asarray(x, dtype='<f4').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(d)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 3, 1, sr, sr * 4, 4, 32))
        f.write(b'data' + struct.pack('<I', len(d)) + d)

def kick(sr):
    t = np.arange(int(0.35 * sr)) / sr; f = 45 + 75 * np.exp(-t / 0.03)
    return 0.9 * np.sin(2 * np.pi * np.cumsum(f) / sr) * np.exp(-t / 0.12)

def snare(sr, rng):
    t = np.arange(int(0.25 * sr)) / sr; n = rng.standard_normal(len(t))
    n = np.convolve(n, [1, -0.6], mode='same')  # tilt up
    return 0.35 * n * np.exp(-t / 0.06) + 0.4 * np.sin(2 * np.pi * 185 * t) * np.exp(-t / 0.05)

def hat(sr, rng):
    t = np.arange(int(0.05 * sr)) / sr; n = rng.standard_normal(len(t))
    return 0.12 * np.diff(n, prepend=0) * np.exp(-t / 0.012)

def loop(bpm, bars, tail_bars, sr):
    rng = np.random.default_rng(1234); beat = 60.0 / bpm; out = np.zeros(int((bars + tail_bars) * 4 * beat * sr) + sr)
    K, S, H = kick(sr), snare(sr, rng), hat(sr, rng)
    def put(x, t):
        i = int(round(t * sr)); out[i:i + len(x)] += x[:max(0, len(out) - i)]
    for b in range(bars):
        t0 = b * 4 * beat
        for k in (0.0, 1.5, 2.0): put(K, t0 + k * beat)        # kick on 1, the "and" of 2, 3
        for s in (1.0, 3.0): put(S, t0 + s * beat)             # snare on 2 and 4
        for h in range(8): put(H, t0 + h * 0.5 * beat)         # 8th hats
    return (0.5 * out / np.abs(out).max()).astype(np.float32)

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('out'); p.add_argument('--bpm', type=float, default=120.0)
    p.add_argument('--bars', type=int, default=4); p.add_argument('--tail-bars', type=int, default=2); p.add_argument('--sr', type=int, default=48000)
    a = p.parse_args(); write_wav(a.out, loop(a.bpm, a.bars, a.tail_bars, a.sr), a.sr)
    print(f'{{"out": "{a.out}", "bpm": {a.bpm}}}')
