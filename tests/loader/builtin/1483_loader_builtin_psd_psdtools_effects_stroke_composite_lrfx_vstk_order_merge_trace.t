#!/bin/sh
# Verify stroke-composite keeps legacy lrFX merge contract when vstk payload is
# parsed in the same layer extra-data stream.
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

test "${trace_output#*builtin PSD: parsed vstk vector stroke style payload*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not parse vstk payload"
    exit 0
}

test "${trace_output#*builtin PSD: legacy lrFX contains glow/bevel/sofi records*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not report legacy lrFX feature records"
    exit 0
}

test "${trace_output#*builtin PSD: merging legacy lrFX effects missing from lfx2*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not merge legacy lrFX effects"
    exit 0
}

test "${trace_output#*builtin PSD: ignoring legacy lrFX when lfx2 is present*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite unexpectedly ignored legacy lrFX"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps lrFX/vstk merge trace contract"
exit 0
