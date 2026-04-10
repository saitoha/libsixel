#!/bin/sh
# Verify infx parse contract remains visible on blend-and-clipping hardcase.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_blend_and_clipping.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "blend-and-clipping decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed infx=0*}" != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping did not parse infx=0"
    exit 0
}

test "${trace_output#*builtin PSD: parsed infx=1*}" != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping did not parse infx=1"
    exit 0
}

echo "ok" 1 - "blend-and-clipping keeps infx parse trace contract"
exit 0
