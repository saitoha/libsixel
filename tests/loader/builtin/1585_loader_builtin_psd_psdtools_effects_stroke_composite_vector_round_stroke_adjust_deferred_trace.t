#!/bin/sh
# Verify deferred round-join path applies strokeAdjust semantics.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_psd_on="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_round_stroke_adjust_on.psd"
trace_output=''
diag_line_on=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd_on}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite round strokeAdjust on decode failed"
    exit 0
}

diag_line_on=${trace_output}
test -n "${diag_line_on}" || {
    echo "not ok" 1 - "effects/stroke-composite missing on diagnostic header"
    exit 0
}

case "${diag_line_on}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite on diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line_on}" in
    *FX_STROKE_ADJUST_BASE*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite did not enable base strokeAdjust code for round join"
        exit 0
        ;;
esac

case "${diag_line_on}" in
    *FX_STROKE_ADJUST_DEFER*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite did not enable deferred strokeAdjust code for round join"
        exit 0
        ;;
esac

echo "ok" 1 - \
    "effects/stroke-composite enables deferred round-join strokeAdjust code"
exit 0
