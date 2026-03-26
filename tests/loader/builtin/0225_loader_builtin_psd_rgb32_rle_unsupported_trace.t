#!/bin/sh
# Verify PSD RGB 32-bit + RLE is explicitly unsupported.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-rgb32-rle-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${tmp_psd}" <<'PY'
import struct
import sys

out = sys.argv[1]
with open(out, "wb") as f:
    f.write(b"8BPS")
    f.write(struct.pack(">H", 1))
    f.write(b"\x00" * 6)
    f.write(struct.pack(">H", 3))
    f.write(struct.pack(">I", 1))
    f.write(struct.pack(">I", 1))
    f.write(struct.pack(">H", 32))
    f.write(struct.pack(">H", 3))  # RGB
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">H", 1))  # RLE
    f.write(b"\x00")
PY

trace_log=$(set +xv; run_img2sixel -L builtin! "${tmp_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported RLE compression for 32-bit RGB"*)
        echo "ok" 1 - "RGB 32-bit RLE unsupported policy is explicit"
        ;;
    *)
        echo "not ok" 1 - "RGB 32-bit RLE unsupported trace is missing"
        exit 0
        ;;
esac

exit 0
