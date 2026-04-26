#!/bin/sh
# TAP test confirming odd-sized inner ANMF chunk requires zero padding.
# Fixture is derived from animated-lossy-alpha-8x8-2frame-min.webp by
# changing first ALPH subchunk size at byte offset 0x48 from 0x0c to 0x0b.
# The implied odd padding byte at offset 0x57 is 0x07 (non-zero), so
# parse_anmf_frame() must fail with W_ERR_VP8_STREAM.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_anmf_inner_odd_padding_nonzero.webp"
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
    echo "not ok" 1 - "builtin WebP animation inner odd padding fixture unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin WebP animation inner odd padding fixture missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation inner odd padding fixture malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8_STREAM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation inner odd padding fixture missing W_ERR_VP8_STREAM"
    exit 0
}

echo "ok" 1 - "builtin WebP animation inner odd padding fixture emits W_ERR_VP8_STREAM"
exit 0
