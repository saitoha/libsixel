#!/bin/sh
# Verify legacy lrFX supplements inactive lfx2 effects on
# effects/stroke-composite hardcase.
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
    echo "not ok" 1 - "effects/stroke-composite missing diagnostic header line"
    exit 0
}

case "${diag_line}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - \
            "effects/stroke-composite diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_LRFX_MERGE*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite did not merge legacy lrFX"
        exit 0
        ;;
esac

case "${diag_line}" in
    *FX_LRFX_IGNORE*)
        echo "not ok" 1 - \
            "effects/stroke-composite still ignored legacy lrFX effects"
        exit 0
        ;;
    *) ;;
esac

echo "ok" 1 - "effects/stroke-composite merges legacy lrFX for inactive lfx2"
exit 0
