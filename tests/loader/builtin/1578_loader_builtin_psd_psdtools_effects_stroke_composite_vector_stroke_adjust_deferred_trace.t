#!/bin/sh
# Verify deferred miter coverage enables stroke-adjust semantics when requested.
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
input_psd_on="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_stroke_adjust_on.psd"
trace_output_off=''
trace_output_on=''
command_status=0

trace_output_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd_off}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite (strokeAdjust off) decode failed"
    exit 0
}

test "${trace_output_off#*builtin PSD: applying deferred stroke-adjusted vector stroke on clipped group*}" \
    = "${trace_output_off}" || {
    echo "not ok" 1 - "effects/stroke-composite unexpectedly enabled deferred stroke-adjust trace"
    exit 0
}

command_status=0
trace_output_on=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd_on}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite (strokeAdjust on) decode failed"
    exit 0
}

test "${trace_output_on#*builtin PSD: applying deferred stroke-adjusted vector stroke on clipped group*}" \
    != "${trace_output_on}" || {
    echo "not ok" 1 - "effects/stroke-composite did not enable deferred stroke-adjust trace"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps deferred strokeAdjust on/off contract"
exit 0
