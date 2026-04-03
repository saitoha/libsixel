#!/bin/sh
# Verify PSD CMYK8 TySh /DefaultStyleSheet malformed payload keeps skip trace.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_malformed_payload.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode -Lbuiltin! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "malformed /DefaultStyleSheet decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"builtin PSD: malformed non-pixel fill payload; skipping layer"*)
        ;;
    *)
        echo "not ok" 1 - "malformed non-pixel fill skip trace is missing"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"builtin PSD: rendering non-pixel fill payload in layer fallback"*)
        echo "not ok" 1 - "malformed /DefaultStyleSheet should not render fill payload"
        exit 0
        ;;
esac

echo "ok" 1 - "PSD CMYK8 TySh /DefaultStyleSheet malformed payload keeps deterministic skip trace"
exit 0
