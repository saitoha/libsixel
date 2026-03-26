#!/bin/sh
# Verify builtin loader decodes PSD RGB 32-bit ZIP.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-rgb32-zip-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${tmp_psd}" <<'PY'
import struct
import sys
import zlib

out = sys.argv[1]
width, height = 2, 2
channels = 3
mode = 3
depth = 32
compression = 2

planes = [
    [0.15, 0.25, 0.35, 0.45],
    [0.25, 0.35, 0.45, 0.55],
    [0.35, 0.45, 0.55, 0.65],
]

plane_bytes = []
for values in planes:
    b = bytearray()
    for v in values:
        b.extend(struct.pack(">f", float(v)))
    plane_bytes.append(bytes(b))

payload = zlib.compress(b"".join(plane_bytes))

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
    echo "not ok" 1 - "PSD RGB 32-bit ZIP decode failed"
    exit 0
}

echo "ok" 1 - "PSD RGB 32-bit ZIP decode succeeds"
exit 0
