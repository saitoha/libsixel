#!/bin/sh
# Verify deferred interior pass keeps bevel-shadow apply after clip-weighting.
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
deferred_trace=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

deferred_trace="${trace_output#*builtin PSD: applying clip-weighted deferred interior effects in layer fallback*}"
test "${deferred_trace}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not enter deferred interior apply"
    exit 0
}

test "${deferred_trace#*builtin PSD: applying bevel shadow in layer fallback*}" \
    != "${deferred_trace}" || {
    echo "not ok" 1 - "effects/stroke-composite did not keep bevel shadow after deferred interior apply"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps deferred bevel-shadow apply contract"
exit 0
