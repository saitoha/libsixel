#!/bin/sh
# Verify CMYK ZIP path keeps bad-ICC failure trace behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

seed_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk8_bad_icc_profile.psd"
tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-cmyk8-zip-badicc-XXXXXX.psd")
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
    raise SystemExit("seed bad ICC profile not found")

width, height = 2, 2
channels = 4
mode = 4
depth = 8
compression = 2

planes = [
    bytes([0x00, 0x80, 0xFF, 0x40]),
    bytes([0x10, 0x20, 0x30, 0x40]),
    bytes([0x50, 0x60, 0x70, 0x80]),
    bytes([0x00, 0x10, 0x20, 0x30]),
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

trace_log=$(set +xv; run_img2sixel -v -Lbuiltin:cms=auto! \
    "${tmp_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: embedded ICC conversion failed"*)
        echo "ok" 1 - "CMYK ZIP bad ICC failure trace is preserved"
        ;;
    *)
        echo "not ok" 1 - "CMYK ZIP bad ICC failure trace is missing"
        exit 0
        ;;
esac

exit 0
