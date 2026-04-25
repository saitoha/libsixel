#!/bin/sh
# TAP test confirming out-of-canvas ANMF rectangle is treated as stream error.
# Fixture is derived from animated-lossless-8x8-2frame-min.webp by setting
# the first ANMF frame X offset to 2 pixels while keeping width=8.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_anmf_subrect_offset.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=webp_decode \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "builtin WebP animation ANMF out-of-canvas check unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin WebP animation ANMF out-of-canvas check missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=1|kind=ERR|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation ANMF out-of-canvas check malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8_STREAM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation ANMF out-of-canvas check missing W_ERR_VP8_STREAM contract code"
    exit 0
}

echo "ok" 1 - "builtin WebP animation ANMF out-of-canvas check emits W_ERR_VP8_STREAM contract code"
exit 0
