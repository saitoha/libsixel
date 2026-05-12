#!/bin/sh
# Verify clbl=1 deferred stroke runs after deferred gradient on group backdrop.
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
gradient_tail=''
stroke_tail=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

gradient_tail="${trace_output#*builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback*}"
test "${gradient_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred gradient overlay trace"
    exit 0
}

stroke_tail="${gradient_tail#*builtin PSD: applying deferred stroke on clipped group*}"
test "${stroke_tail}" != "${gradient_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred stroke trace after deferred gradient"
    exit 0
}

test "${stroke_tail#*builtin PSD: using post-effect offscreen group backdrop for deferred stroke blend*}" \
    != "${stroke_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing post-effect backdrop trace after deferred stroke"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite runs deferred stroke after post-effect gradient backdrop"
exit 0
