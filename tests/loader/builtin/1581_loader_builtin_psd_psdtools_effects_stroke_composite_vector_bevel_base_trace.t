#!/bin/sh
# Verify base fallback applies bevel-join vector-stroke coverage.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite_bevel.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite bevel decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying bevel-join vector stroke coverage in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite did not apply base bevel-join vector stroke coverage"
    exit 0
}

test "${trace_output#*builtin PSD: applying miter-join vector stroke coverage in layer fallback*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite unexpectedly used base miter-join trace for bevel join"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite keeps base bevel-join vector stroke coverage contract"
exit 0
