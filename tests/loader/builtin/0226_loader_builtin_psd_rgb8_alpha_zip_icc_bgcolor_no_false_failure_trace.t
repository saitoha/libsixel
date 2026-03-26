#!/bin/sh
# Verify builtin PSD RGB+alpha ZIP with embedded ICC and --bgcolor avoids false failure trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

seed_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_rgb8_alpha_embedded_esrgb.psd"
tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-rgb8a-zip-icc-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${seed_psd}" "${tmp_psd}" <<'PY'
import struct
import sys
import zlib

seed_path = sys.argv[1]
out_path = sys.argv[2]

with open(seed_path, "rb") as f:
    seed = f.read()

if seed[:4] != b"8BPS":
    raise SystemExit("seed PSD signature mismatch")

def u16be(buf, off):
    return struct.unpack_from(">H", buf, off)[0]

def u32be(buf, off):
    return struct.unpack_from(">I", buf, off)[0]

def extract_icc(buf):
    off = 26
    color_mode_len = u32be(buf, off)
    off += 4 + color_mode_len
    res_len = u32be(buf, off)
    off += 4
    end = off + res_len
    while off + 12 <= end:
        sig = buf[off:off + 4]
        off += 4
        if sig != b"8BIM":
            break
        rid = u16be(buf, off)
        off += 2
        name_len = buf[off]
        off += 1 + name_len
        if ((1 + name_len) & 1) != 0:
            off += 1
        data_len = u32be(buf, off)
        off += 4
        data = buf[off:off + data_len]
        off += data_len
        if (data_len & 1) != 0:
            off += 1
        if rid == 0x040F:
            return data
    return None

icc = extract_icc(seed)
if not icc:
    raise SystemExit("seed ICC profile not found")

width, height = 2, 2
channels = 4
mode = 3
depth = 8
compression = 2

planes = [
    bytes([255, 0, 255, 0]),
    bytes([0, 255, 255, 0]),
    bytes([0, 0, 255, 255]),
    bytes([255, 128, 64, 0]),
]
payload = zlib.compress(b"".join(planes))

res = bytearray()
res.extend(b"8BIM")
res.extend(struct.pack(">H", 0x040F))
res.extend(b"\x00")
res.extend(b"\x00")
res.extend(struct.pack(">I", len(icc)))
res.extend(icc)
if (len(icc) & 1) != 0:
    res.extend(b"\x00")

with open(out_path, "wb") as f:
    f.write(b"8BPS")
    f.write(struct.pack(">H", 1))
    f.write(b"\x00" * 6)
    f.write(struct.pack(">H", channels))
    f.write(struct.pack(">I", height))
    f.write(struct.pack(">I", width))
    f.write(struct.pack(">H", depth))
    f.write(struct.pack(">H", mode))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", len(res)))
    f.write(res)
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">H", compression))
    f.write(payload)
PY

command_status=0
command_output=$(set +xv; run_img2sixel -Lbuiltin:cms=auto! \
    -B "#112233" "${tmp_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin ZIP ICC decode failed: ${command_output}"
    exit 0
}

printf '%s\n' "${command_output}" | grep -F "embedded ICC conversion failed" >/dev/null && {
    echo "not ok" 1 - "unexpected embedded ICC failure trace was emitted on ZIP path"
    exit 0
}

echo "ok" 1 - "builtin PSD RGB+alpha ZIP ICC path avoids false failure trace"
exit 0
