#!/bin/sh
# Verify non-pixel layer payload in multi-layer PSD is rejected with
# deterministic unsupported trace.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_multilayer_nonpixel_tysh.psd"

trace_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! "${input_psd}" -o/dev/null 2>&1 || true)

case "${trace_output}" in
    *"builtin PSD: unsupported non-pixel layer in layer fallback"*)
        echo "ok" 1 - "non-pixel multi-layer payload is rejected as unsupported"
        ;;
    *)
        echo "not ok" 1 - "non-pixel multi-layer unsupported trace is missing"
        ;;
esac
