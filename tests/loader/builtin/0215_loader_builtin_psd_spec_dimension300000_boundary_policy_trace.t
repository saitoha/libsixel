#!/bin/sh
# Verify PSD dimension boundary 300000 is accepted before policy checks.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

tmp_psd=$(mktemp "${TMPDIR:-/tmp}/libsixel-psd-dim300000-XXXXXX.psd")
trap 'rm -f "${tmp_psd}"' EXIT HUP INT TERM

python3 - "${tmp_psd}" <<'PY'
import struct
import sys

out = sys.argv[1]
with open(out, "wb") as f:
    f.write(b"8BPS")
    f.write(struct.pack(">H", 1))
    f.write(b"\x00" * 6)
    f.write(struct.pack(">H", 1))
    f.write(struct.pack(">I", 1))
    f.write(struct.pack(">I", 300000))
    f.write(struct.pack(">H", 8))
    f.write(struct.pack(">H", 7))  # Multichannel policy path
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">I", 0))
    f.write(struct.pack(">H", 0))
    f.write(b"\x00")
PY

trace_log=$(set +xv; run_img2sixel -L builtin! "${tmp_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported color mode (7: Multichannel)"*)
        echo "ok" 1 - "dimension=300000 passes boundary and reaches policy"
        ;;
    *)
        echo "not ok" 1 - "dimension=300000 did not pass boundary validation"
        exit 0
        ;;
esac

exit 0
