#!/bin/sh
# TAP test confirming VP8 static/anim error contracts no longer emit
# W_UNSUP_VP8_FEATURE or W_UNSUP_VP8_ALPHA.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_static="${TOP_SRCDIR}/tests/data/corrupted/bad_vp8_interframe_flag.webp"
input_anim="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_vp8_interframe_flag.webp"
trace_static=''
trace_anim=''
diag_static=''
diag_anim=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_static=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_static}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp unexpectedly succeeded"
    exit 0
}

diag_static=${trace_static#*LSXWEBP1\|}
test "${diag_static}" != "${trace_static}" || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp missing LSXWEBP1 contract header"
    exit 0
}

diag_static="LSXWEBP1|${diag_static}"
diag_static=${diag_static%%"${nl}"*}

test "${diag_static#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_static}" || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp malformed error contract header"
    exit 0
}

test "${diag_static#*W_ERR_VP8_STREAM*}" != "${diag_static}" || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp missing W_ERR_VP8_STREAM"
    exit 0
}

test "${diag_static#*W_UNSUP_VP8_FEATURE*}" = "${diag_static}" || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp still emitted W_UNSUP_VP8_FEATURE"
    exit 0
}

test "${diag_static#*W_UNSUP_VP8_ALPHA*}" = "${diag_static}" || {
    echo "not ok" 1 - "forced builtin loader bad_vp8_interframe_flag.webp still emitted W_UNSUP_VP8_ALPHA"
    exit 0
}

command_status=0
trace_anim=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_anim}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp unexpectedly succeeded"
    exit 0
}

diag_anim=${trace_anim#*LSXWEBP1\|}
test "${diag_anim}" != "${trace_anim}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp missing LSXWEBP1 contract header"
    exit 0
}

diag_anim="LSXWEBP1|${diag_anim}"
diag_anim=${diag_anim%%"${nl}"*}

test "${diag_anim#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_anim}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp malformed error contract header"
    exit 0
}

test "${diag_anim#*W_ERR_VP8_STREAM*}" != "${diag_anim}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp missing W_ERR_VP8_STREAM"
    exit 0
}

test "${diag_anim#*W_UNSUP_VP8_FEATURE*}" = "${diag_anim}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp still emitted W_UNSUP_VP8_FEATURE"
    exit 0
}

test "${diag_anim#*W_UNSUP_VP8_ALPHA*}" = "${diag_anim}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8_interframe_flag.webp still emitted W_UNSUP_VP8_ALPHA"
    exit 0
}

echo "ok" 1 - "forced builtin loader VP8 static/anim error contracts no longer emit W_UNSUP_VP8_*"
exit 0
