#!/bin/sh
# Verify layer-only RGB8 PSD with multiple layers is deterministically rejected
# by single-layer fallback layout policy.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_multilayer.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: unsupported layer fallback layout"*)
        echo "ok" 1 - "layer-only RGB8 PSD with multiple layers is rejected by fallback layout policy"
        ;;
    *"builtin PSD: malformed layer record geometry"*)
        echo "ok" 1 - "layer-only RGB8 malformed geometry is diagnosed as malformed"
        ;;
    *)
        echo "not ok" 1 - "multi-layer RGB8 fallback layout trace is missing"
        exit 0
        ;;
esac

exit 0
