#!/bin/sh
# Verify ebbl layer-effect object is parsed in PSD fallback metadata pass.
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
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/shape-fx2 decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed ebbl bevel object in layer effects*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 did not parse ebbl bevel object"
    exit 0
}

echo "ok" 1 - "effects/shape-fx2 keeps ebbl bevel parse contract"
exit 0
