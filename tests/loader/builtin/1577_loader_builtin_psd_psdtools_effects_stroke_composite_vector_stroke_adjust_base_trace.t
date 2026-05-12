#!/bin/sh
# Verify base miter coverage enables stroke-adjust semantics when requested.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download
#   python3 - <<'PY'
# from pathlib import Path
# src = Path('tests/data/psd-tools/psdtools_effects_stroke_composite.psd')
# dst = Path('tests/data/psd-tools/psdtools_effects_stroke_composite_stroke_adjust_on.psd')
# dst.write_bytes(src.read_bytes().replace(
#     b'strokeStyleStrokeAdjustbool\\x00',
#     b'strokeStyleStrokeAdjustbool\\x01'))
# PY

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_psd_off="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
trace_output=''
diag_line_off=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd_off}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite (strokeAdjust off) decode failed"
    exit 0
}

diag_line_off=${trace_output}
test -n "${diag_line_off}" || {
    echo "not ok" 1 - "effects/stroke-composite missing off diagnostic header"
    exit 0
}

case "${diag_line_off}" in
    LSXPSD1\|rc=0\|kind=OK\|codes=*) ;;
    *)
        echo "not ok" 1 - "effects/stroke-composite off diagnostic header is malformed"
        exit 0
        ;;
esac

case "${diag_line_off}" in
    *FX_STROKE_ADJUST_BASE*)
        echo "not ok" 1 - "effects/stroke-composite unexpectedly enabled base stroke-adjust code"
        exit 0
        ;;
    *) ;;
esac

case "${diag_line_off}" in
    *FX_STROKE_ADJUST_DEFER*)
        echo "not ok" 1 - "effects/stroke-composite unexpectedly enabled deferred stroke-adjust code"
        exit 0
        ;;
    *) ;;
esac

echo "ok" 1 - "effects/stroke-composite keeps strokeAdjust disabled in off fixture"
exit 0
