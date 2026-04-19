#!/bin/sh
# Verify dual-stroke overlap keeps union alpha and mode-aware color blend.
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
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying dual-stroke union coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite lost base dual-stroke union alpha contract"
    exit 0
}

test "${trace_output#*builtin PSD: applying deferred dual-stroke union on clipped group*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite lost deferred dual-stroke union alpha contract"
    exit 0
}

test "${trace_output#*builtin PSD: applying mode-aware dual-stroke blend in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not keep base mode-aware overlap blend"
    exit 0
}

test "${trace_output#*builtin PSD: applying mode-aware dual-stroke blend on clipped group*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not keep deferred mode-aware overlap blend"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps dual-stroke overlap non-regression contract"
exit 0
