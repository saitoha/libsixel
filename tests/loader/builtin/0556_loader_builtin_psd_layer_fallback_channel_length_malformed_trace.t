#!/bin/sh
# Verify layer fallback rejects channel-length<2 with deterministic trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_single_layer_channel_length1.psd"
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" -o /dev/null 2>&1 || true)

case "${trace_log}" in
    *"builtin PSD: malformed layer channel length"*)
        echo "ok" 1 - "layer fallback channel-length<2 is malformed"
        ;;
    *)
        echo "not ok" 1 - "malformed layer channel-length trace is missing"
        exit 0
        ;;
esac

exit 0
