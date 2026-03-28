#!/bin/sh
# Verify builtin PSD rejects unknown compression id with deterministic trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_compression4_rgb.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported compression (4)"*)
        echo "ok" 1 - "compression id 4 is rejected as unsupported"
        ;;
    *)
        echo "not ok" 1 - "unsupported compression trace is missing"
        exit 0
        ;;
esac

exit 0
