#!/bin/sh
# Verify PSD Lab 32-bit + RLE is explicitly unsupported.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_lab32_rle.psd"

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported RLE compression for 32-bit Lab"*)
        echo "ok" 1 - "Lab 32-bit RLE unsupported policy is explicit"
        ;;
    *)
        echo "not ok" 1 - "Lab 32-bit RLE unsupported trace is missing"
        exit 0
        ;;
esac

exit 0
