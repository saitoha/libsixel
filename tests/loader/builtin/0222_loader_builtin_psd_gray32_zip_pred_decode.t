#!/bin/sh
# Verify builtin loader decodes PSD Gray 32-bit ZIP+Prediction.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-gray32-zippred-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${tmp_psd}" <<'PY'
import struct
import sys
import zlib

out = sys.argv[1]
width, height = 2, 2
channels = 1
mode = 1
depth = 32
compression = 3
values = [0.05, 0.15, 0.25, 0.35]

raw = bytearray()
for v in values:
    raw.extend(struct.pack(">f", float(v)))

pred = bytearray(raw)
row_bytes = width * 4
for y in range(height):
    row_off = y * row_bytes
    prev = int.from_bytes(raw[row_off:row_off + 4], "big")
    pred[row_off:row_off + 4] = prev.to_bytes(4, "big")
    for x in range(1, width):
        off = row_off + x * 4
        cur = int.from_bytes(raw[off:off + 4], "big")
        enc = (cur - prev) & 0xffffffff
        pred[off:off + 4] = enc.to_bytes(4, "big")
        prev = cur

payload = zlib.compress(bytes(pred))

with open(out, "wb") as f:
    f.write(b"8BPS")
    f.write(struct.pack(">H", 1))
    f.write(b"\x00" * 6)
    f.write(struct.pack(">H", channels))
    f.write(struct.pack(">I", height))
    f.write(struct.pack(">I", width))
    f.write(struct.pack(">H", depth))
    f.write(struct.pack(">H", mode))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">H", compression))
    f.write(payload)
PY

run_img2sixel -L builtin! "${tmp_psd}" >/dev/null || {
    echo "not ok" 1 - "PSD Gray 32-bit ZIP+Prediction decode failed"
    exit 0
}

echo "ok" 1 - "PSD Gray 32-bit ZIP+Prediction decode succeeds"
exit 0
