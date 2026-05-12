#!/bin/sh
# Verify malformed lrFX record signature does not emit legacy feature-record
# trace and decode keeps fallback behavior.

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/psdtools_effects_shape_fx2_lrfx_bad_recordsig.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o /dev/null "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/shape-fx2 malformed-lrFX decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed clbl=1*}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 malformed-lrFX fixture did not reach layer-effects parse path"
    exit 0
}

test "${trace_output#*builtin PSD: legacy lrFX contains glow/bevel/sofi records*}" \
    = "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx2 malformed lrFX unexpectedly reported feature records"
    exit 0
}

echo "ok" 1 - "effects/shape-fx2 malformed lrFX does not emit feature-record trace"
exit 0
