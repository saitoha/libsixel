#!/bin/sh
# Verify solid overlay effect apply trace remains visible on
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
command_status=0
overlay_trace_ok=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying solid overlay effect in layer fallback*}" \
    != "${trace_output}" && overlay_trace_ok=1
test "${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}" \
    != "${trace_output}" && overlay_trace_ok=1

test "${overlay_trace_ok}" = "1" || {
    echo "not ok" 1 - "effects/stroke-composite solid-overlay trace contract is missing"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps solid-overlay trace contract"
exit 0
