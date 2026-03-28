#!/bin/sh
# Verify layer-marked mode7 CMYK16 PSD is deterministically rejected as unsupported
# when merged/composite image data is missing.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk16_missing_composite_marker.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported file without merged/composite image"*)
        echo "ok" 1 - "mode7 CMYK16 missing-composite marker is rejected as unsupported"
        ;;
    *)
        echo "not ok" 1 - "mode7 CMYK16 missing-composite unsupported trace is missing"
        exit 0
        ;;
esac

exit 0
