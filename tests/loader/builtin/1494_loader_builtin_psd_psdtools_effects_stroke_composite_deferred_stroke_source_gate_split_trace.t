#!/bin/sh
# Verify clbl=1 deferred stroke keeps source/gate split diagnostics.
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

test "${trace_output#*builtin PSD: applying deferred stroke on clipped group*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not apply deferred stroke"
    exit 0
}

test "${trace_output#*builtin PSD: applying deferred stroke with base silhouette coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite lost base silhouette deferred stroke coverage"
    exit 0
}

test "${trace_output#*builtin PSD: applying fractional silhouette coverage for deferred effect stroke in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite lost deferred stroke fractional coverage trace"
    exit 0
}

test "${trace_output#*builtin PSD: separating deferred stroke coverage source and clip gate in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite lost deferred stroke source/gate split trace"
    exit 0
}

test "${trace_output#*builtin PSD: suppressing synthesized vector stroke on clipping-group base layer*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite unexpectedly suppressed clbl=1 vector stroke"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps clbl=1 deferred stroke source/gate split contract"
exit 0
