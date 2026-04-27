#!/bin/sh
# Verify FXPRI inside overlap is applied after deferred overlap decomposition.
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
overlap_tail=''
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

overlap_tail="${trace_output#*builtin PSD: applying deferred dual-stroke overlap decomposition on clipped group*}"
test "${overlap_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred dual-stroke overlap decomposition trace"
    exit 0
}

test "${overlap_tail#*builtin PSD: applying effect-priority dual-stroke overlap on clipped group*}" \
    != "${overlap_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing FXPRI overlap trace after deferred overlap decomposition"
    exit 0
}

echo "ok" 1 - \
    "effects/stroke-composite applies FXPRI overlap inside deferred overlap decomposition flow"
exit 0
