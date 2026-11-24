#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy"]
# ///
"""Normalise float32 WAVs (44-byte header) to -1 dBFS peak, in place. Used by s8.sh."""
import sys, numpy as np
for p in sys.argv[1:]:
    b = open(p, 'rb').read(); x = np.frombuffer(b[44:], dtype='<f4'); g = 10 ** (-1 / 20) / max(np.abs(x).max(), 1e-9)
    open(p, 'wb').write(b[:44] + (x * g).astype('<f4').tobytes())
