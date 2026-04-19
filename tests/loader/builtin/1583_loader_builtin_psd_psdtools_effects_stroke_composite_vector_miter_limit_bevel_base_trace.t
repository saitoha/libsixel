#!/bin/sh
# Verify low miter-limit fallback keeps bevel coverage trace in base path.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_miter_limit_1p5.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite low miter-limit decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying miter-limit constrained vector stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not report base miter-limit constrained vector coverage"
    exit 0
}

test "${trace_output#*builtin PSD: applying bevel-join vector stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not switch base miter-limit fallback to bevel coverage"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps base miter-limit bevel fallback coverage contract"
exit 0
