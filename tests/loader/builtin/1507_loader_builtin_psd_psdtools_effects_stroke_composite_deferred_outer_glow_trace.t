#!/bin/sh
# Verify stroke-composite applies deferred outer glow in clbl=1 groups.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite missing diagnostic header"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite malformed diagnostic header"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_CLBL1_DEFER_INTERIOR*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite missing FX_CLBL1_DEFER_INTERIOR"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_DEFERRED_OUTER_CLIP*)
        echo "not ok" 1 - \
            "effects/stroke-composite unexpectedly emitted FX_DEFERRED_OUTER_CLIP"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_ORGL_INACTIVE_PARSE*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite missing FX_ORGL_INACTIVE_PARSE"
        exit 0
        ;;
esac

echo "ok" 1 - \
    "effects/stroke-composite keeps deferred outer inactive code contract"
exit 0
