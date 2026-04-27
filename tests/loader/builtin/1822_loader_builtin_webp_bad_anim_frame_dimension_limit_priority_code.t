#!/bin/sh
# TAP test confirming ANIM limit priority prefers frame limit over dimension.
# Derived fixture: bad_anim_frame_count_exceeds_limit.webp
# Patched offsets: VP8X width_minus_one bytes at 24..26 -> ff 7f 00.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_frame_and_dimension_exceeds_limit.webp"
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
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow malformed error contract header"
    exit 0
}

test "${diag_line#*W_UNSUP_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow missing W_UNSUP_ANIM"
    exit 0
}

test "${diag_line#*W_UNSUP_ANIM_FRAME_LIMIT*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow missing W_UNSUP_ANIM_FRAME_LIMIT"
    exit 0
}

test "${diag_line#*W_UNSUP_ANIM_DIMENSION_LIMIT*}" = "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow unexpectedly emitted W_UNSUP_ANIM_DIMENSION_LIMIT"
    exit 0
}

test "${diag_line#*W_UNSUP_ANIM_PIXEL_LIMIT*}" = "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader fixture with frame+dimension overflow unexpectedly emitted W_UNSUP_ANIM_PIXEL_LIMIT"
    exit 0
}

echo "ok" 1 - "forced builtin loader fixture with frame+dimension overflow emits frame-limit reason only"
exit 0
