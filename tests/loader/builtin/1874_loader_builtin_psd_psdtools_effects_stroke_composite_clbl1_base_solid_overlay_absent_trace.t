#!/bin/sh
# Verify clbl=1 deferred ownership suppresses base solid overlay apply.
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
suppressed_tail=''
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

test "${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 overlay suppression trace"
    exit 0
}

test "${trace_output#*builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred gradient overlay trace"
    exit 0
}

suppressed_tail="${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}"

test "${suppressed_tail#*builtin PSD: applying solid overlay effect in layer fallback*}" \
    = "${suppressed_tail}" || {
    echo "not ok" 1 - "effects/stroke-composite unexpectedly applied base solid overlay"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite suppresses base solid overlay under clbl=1 deferred ownership"
exit 0
