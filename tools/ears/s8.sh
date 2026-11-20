#!/bin/sh
# S8 listening set: synthetic drum loop through the tape engine at several settings. Output: build/s8/*.wav (gitignored).
# Use a real loop instead: S8_IN=/path/loop.wav S8_BPM=97 make s8   (one set at that tempo)
set -e
cd "$(dirname "$0")/../.."
O=build/s8; mkdir -p $O
R=build/tidbank_render
loop() { if [ -n "$S8_IN" ]; then echo "$S8_IN"; else uv run --quiet --with numpy tools/ears/drumloop.py $O/loop_$1.wav --bpm $1 >/dev/null; echo $O/loop_$1.wav; fi; }
r() { name=$1; bpm=$2; shift 2; $R --engine tape --in "$(loop $bpm)" --out $O/$name.wav --bpm $bpm --dry 1 --wet 0.7 "$@" >/dev/null; echo "  $name"; }
BPMS=${S8_BPM:-"90 120 140"}
echo "== synced, 1/8 on head 1, mode 1, defaults"
for b in $BPMS; do r 01_sync_${b}bpm $b --div 0.5 --params mode=1; done
B=${S8_BPM:-120}
echo "== mode 7 (all heads, even spacing) and vintage spacing"
r 02_mode7_even $B --div 0.5 --params mode=7
r 03_mode7_vintage $B --div 0.5 --params mode=7,spacing=1
echo "== nudge -50 / +50 cents (detuned from tempo)"
r 04_nudge_minus50 $B --div 0.5 --params mode=1,nudge_cents=-50
r 05_nudge_plus50 $B --div 0.5 --params mode=1,nudge_cents=50
echo "== feedback HPF 20 / 80 / 300 Hz, intensity 0.8, mode 4"
for h in 20 80 300; do r 06_hpf_${h}hz $B --div 0.5 --params mode=4,intensity=0.8,fb_hpf=$h; done
echo "== tempo change 2 s in (to 0.8x), inertia 300 ms vs 2 s: tape glide"
r 07_tempo_glide_300ms $B --div 0.5 --params mode=7,intensity=0.6 --sched "2.0:bpm=$(python3 -c "print($B*0.8)")"
r 08_tempo_glide_2000ms $B --div 0.5 --params mode=7,intensity=0.6,inertia_ms=2000 --sched "2.0:bpm=$(python3 -c "print($B*0.8)")"
echo "== drive 0 / +12 dB, intensity 0.9; runaway at intensity 1.1"
r 09_drive_0 $B --div 0.5 --params mode=4,intensity=0.9,drive=0
r 10_drive_12 $B --div 0.5 --params mode=4,intensity=0.9,drive=12
r 11_runaway_1.1 $B --div 0.5 --params mode=4,intensity=1.1 --sched "9.0:intensity=0.5"
# float WAVs above 0 dBFS clip in most players: normalise each file to -1 dBFS peak (loudness between files is NOT kept)
uv run --quiet --with numpy python3 - $O/[01]*.wav <<'PY'
import sys, numpy as np
for p in sys.argv[1:]:
    b = open(p, 'rb').read(); x = np.frombuffer(b[44:], dtype='<f4'); g = 10 ** (-1 / 20) / max(np.abs(x).max(), 1e-9)
    open(p, 'wb').write(b[:44] + (x * g).astype('<f4').tobytes())
PY
echo "done (each file normalised to -1 dBFS peak): $(pwd)/$O"
