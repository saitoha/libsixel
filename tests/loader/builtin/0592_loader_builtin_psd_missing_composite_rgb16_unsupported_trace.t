#!/bin/sh
# Verify malformed layer-marker RGB16 PSD is deterministically rejected
# by layer-fallback layout checks.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb16_missing_composite_marker.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported layer fallback layout"*)
        echo "ok" 1 - "RGB16 marker fixture is rejected by layer-fallback layout checks"
        ;;
    *)
        echo "not ok" 1 - "rgb16 layer-fallback layout unsupported trace is missing"
        exit 0
        ;;
esac

exit 0
