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
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite round decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying round-join vector stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite round fixture lost base vector-stroke join path"
    exit 0
}

test "${trace_output#*builtin PSD: applying deferred round-join vector stroke on clipped group*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite round fixture lost deferred vector-stroke join path"
    exit 0
}

test "${trace_output#*builtin PSD: skipping vector stroke cap on closed silhouette in layer fallback*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite round fixture unexpectedly disabled vector cap path"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps vector-stroke cap non-regression contract"
exit 0
