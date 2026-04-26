#!/bin/sh
# TAP test confirming ANMF mixed VP8 and VP8L chunks map to VP8 stream error.
# Fixture is derived from animated-lossy-alpha-8x8-2frame-min.webp by patching
# first ANMF inner ALPH FourCC at byte offset 68 to VP8L.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_anmf_mixed_vp8_vp8l_chunks.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "builtin ANMF mixed VP8/VP8L fixture unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin ANMF mixed VP8/VP8L fixture missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin ANMF mixed VP8/VP8L fixture malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8_STREAM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin ANMF mixed VP8/VP8L fixture missing W_ERR_VP8_STREAM"
    exit 0
}

echo "ok" 1 - "builtin ANMF mixed VP8/VP8L fixture emits W_ERR_VP8_STREAM"
exit 0
