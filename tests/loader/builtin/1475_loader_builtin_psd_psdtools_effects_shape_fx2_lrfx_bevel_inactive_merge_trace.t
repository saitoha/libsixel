#!/bin/sh
# Verify inactive ebbl targets in lfx2 still allow legacy lrFX completion.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_shape_fx2.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/shape-fx2 decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed ebbl bevel object in layer effects (inactive)*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 missing inactive ebbl parse trace"
    exit 0
}

test "${trace_output#*builtin PSD: merging legacy lrFX effects missing from lfx2*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 did not merge legacy lrFX effects"
    exit 0
}

test "${trace_output#*builtin PSD: ignoring legacy lrFX when lfx2 is present*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 unexpectedly ignored legacy lrFX"
    exit 0
}

echo "ok" 1 - "effects/shape-fx2 keeps inactive ebbl on legacy completion path"
exit 0
