#!/bin/sh
# Verify builtin loader decodes PSD Lab 32-bit ZIP+Prediction.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-lab32-zippred-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${tmp_psd}" <<'PY'
import struct
import sys
import zlib

out = sys.argv[1]
width, height = 2, 2
channels = 3
mode = 9
depth = 32
compression = 3

planes = [
    [20.0, 40.0, 60.0, 80.0],      # L*
    [-32.0, -12.0, 8.0, 28.0],     # a*
    [-20.0, 4.0, 16.0, 24.0],      # b*
]

def encode_plane(values):
    b = bytearray()
    for v in values:
        b.extend(struct.pack(">f", float(v)))
    return bytes(b)

def encode_prediction_32(plane, width, height):
    out = bytearray(plane)
    row_bytes = width * 4
    for y in range(height):
        row_off = y * row_bytes
        prev = int.from_bytes(plane[row_off:row_off + 4], "big")
        out[row_off:row_off + 4] = prev.to_bytes(4, "big")
        for x in range(1, width):
            off = row_off + x * 4
            cur = int.from_bytes(plane[off:off + 4], "big")
            pred = (cur - prev) & 0xffffffff
            out[off:off + 4] = pred.to_bytes(4, "big")
            prev = cur
    return bytes(out)

encoded_planes = []
for values in planes:
    raw = encode_plane(values)
    encoded_planes.append(encode_prediction_32(raw, width, height))

payload = zlib.compress(b"".join(encoded_planes))

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
    echo "not ok" 1 - "PSD Lab 32-bit ZIP+Prediction decode failed"
    exit 0
}

echo "ok" 1 - "PSD Lab 32-bit ZIP+Prediction decode succeeds"
exit 0
