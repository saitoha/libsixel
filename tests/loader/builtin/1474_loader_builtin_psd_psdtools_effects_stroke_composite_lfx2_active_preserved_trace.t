#!/bin/sh
# Verify active lfx2 channels remain active after lrFX inactive completion.
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
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: merging legacy lrFX effects missing from lfx2*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite did not merge legacy lrFX effects"
    exit 0
}

test "${trace_output#*builtin PSD: parsed GrFl effect object in layer effects*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite lost active lfx2 GrFl parsing"
    exit 0
}

test "${trace_output#*builtin PSD: parsed GrFl effect object in layer effects \(inactive\)*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite regressed GrFl into inactive state"
    exit 0
}

echo "ok" 1 - "effects/stroke-composite keeps active lfx2 channels during lrFX completion"
exit 0
