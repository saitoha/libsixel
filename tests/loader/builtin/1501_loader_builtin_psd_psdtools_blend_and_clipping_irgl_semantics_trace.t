#!/bin/sh
# Verify blend-and-clipping exposes IrGl source/choke/range semantics trace.
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
    -Lbuiltin:e=auto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "blend-and-clipping decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed IrGl glow source/choke/range semantics*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping missed IrGl source/choke/range semantics trace"
    exit 0
}

test "${trace_output#*builtin PSD: parsed IrGl effect object in layer effects \(inactive\)*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend-and-clipping lost IrGl inactive parse trace"
    exit 0
}

echo "ok" 1 - "blend-and-clipping keeps IrGl source/choke/range semantics trace"
exit 0
