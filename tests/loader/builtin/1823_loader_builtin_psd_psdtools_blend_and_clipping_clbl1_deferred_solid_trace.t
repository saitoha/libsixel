#!/bin/sh
# Verify clbl=1 deferred solid overlay keeps trace contracts.
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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_blend_and_clipping.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

: "${trace_output}"

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "blend_and_clipping decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: applying clip-weighted deferred solid overlay in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend_and_clipping missing deferred solid overlay trace"
    exit 0
}

test "${trace_output#*builtin PSD: keeping deferred solid overlay alpha unchanged in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "blend_and_clipping missing deferred solid alpha-invariant trace"
    exit 0
}

echo "ok" 1 - "blend_and_clipping keeps deferred solid overlay traces"
exit 0
