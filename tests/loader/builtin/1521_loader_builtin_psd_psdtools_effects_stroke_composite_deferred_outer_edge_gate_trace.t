#!/bin/sh
# Verify deferred outer edge source keeps exterior-connected gate in clbl=1.
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
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying deferred outer distance-band coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missed outer distance-band path"
    exit 0
}

test "${trace_output#*builtin PSD: gating deferred outer effects with exterior background in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missed exterior edge gate"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps deferred outer edge-gate contract"
exit 0
