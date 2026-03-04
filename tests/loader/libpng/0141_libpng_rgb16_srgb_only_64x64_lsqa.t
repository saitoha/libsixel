#!/bin/sh
# TAP test: libpng should decode 16-bit sRGB PNG (without iCCP/gAMA/cHRM)
# and keep visual parity against the 64x64 PNM reference.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake_64_rgb16_srgb_only.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/libpng_expected/0141_libpng_rgb16_srgb_only_64x64_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_64_rgb16_srgb_only.sixel"

python3 - "$input_png" <<'PY'
import struct
import sys

path = sys.argv[1]
with open(path, "rb") as f:
    payload = f.read()
if payload[:8] != b"\x89PNG\r\n\x1a\n":
    raise SystemExit("fixture is not a png")

offset = 8
chunk_names = []
width = 0
height = 0
bit_depth = 0
while offset + 12 <= len(payload):
    length = int.from_bytes(payload[offset:offset + 4], "big")
    chunk_type = payload[offset + 4:offset + 8].decode("ascii")
    chunk_data = payload[offset + 8:offset + 8 + length]
    chunk_names.append(chunk_type)
    if chunk_type == "IHDR":
        width, height, bit_depth = struct.unpack(">IIB", chunk_data[:9])
    offset += 12 + length
    if chunk_type == "IEND":
        break

if width != 64 or height != 64:
    raise SystemExit(f"fixture size mismatch: {width}x{height}")
if bit_depth != 16:
    raise SystemExit(f"fixture bit depth mismatch: {bit_depth}")
if "sRGB" not in chunk_names:
    raise SystemExit("sRGB chunk missing")
for banned in ("iCCP", "gAMA", "cHRM"):
    if banned in chunk_names:
        raise SystemExit(f"unexpected chunk present: {banned}")
PY

run_img2sixel -Llibpng:enable_cms=0! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.99" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "libpng rgb16 sRGB-only fixture matches 64x64 reference"
exit 0
