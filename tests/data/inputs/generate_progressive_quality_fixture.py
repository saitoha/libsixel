#!/usr/bin/env python3
"""Generate a large anchored capture for decoder MS-SSIM integration tests.

Run from any directory after building converters/img2sixel. The independent
reference is snake_192.png, resampled from images/snake.png as documented in

docs/quality/measurement-policy.md. No progressive encoder is involved:
the first pass is explicitly black, and the second is ordinary SIXEL.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DIRECTORY = Path(__file__).resolve().parent
encoded = subprocess.check_output([
    str(ROOT / "converters/img2sixel"), str(DIRECTORY / "snake_192.png")])
match = re.fullmatch(rb"\x1bP[0-9;]*q(.*)\x1b\\", encoded, re.S)
if match is None:
    raise ValueError("expected one ordinary seven-bit SIXEL DCS")
# A 192-row canvas contains exactly 32 six-pixel bands. The black baseline
# covers every pixel; P2=1 on both passes preserves the shared destination.
baseline = (b'\x1bP0;1q"1;1;192;192#0;2;0;0;0' +
            b'-'.join([b'!192~'] * 32) + b'\x1b\\')
correction = b'\x1bP0;1q' + match[1] + b'\x1b\\'
(DIRECTORY / "progressive-snake-192-baseline.six").write_bytes(baseline)
(DIRECTORY / "progressive-snake-192.six").write_bytes(
    b'\x1b7' + baseline + b'\x1b8' + correction)
