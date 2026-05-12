#!/bin/sh
# Verify vector-stroke cap path remains active with mode-aware dual blend.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_round.psd"
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
    echo "not ok" 1 - "effects/stroke-composite round decode failed"
    exit 0
}

diag_line=${trace_output%%"${nl}"*}
test -n "${diag_line}" || {
    echo "not ok" 1 - "effects/stroke-composite missing diagnostic header line"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_VECTOR_ROUND_BASE*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite round fixture lost base vector-stroke join path"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_VECTOR_ROUND_DEFER*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite round fixture lost deferred vector-stroke join path"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_VECTOR_CAP_SKIP*)
        echo "not ok" 1 - "effects/stroke-composite round fixture unexpectedly disabled vector cap path"
        exit 0
        ;;
    *) ;;
esac

echo "ok" 1 - "effects/stroke-composite keeps vector-stroke cap non-regression contract"
exit 0
