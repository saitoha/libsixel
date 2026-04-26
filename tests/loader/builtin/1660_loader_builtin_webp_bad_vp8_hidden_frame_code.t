#!/bin/sh
# TAP test confirming hidden VP8 frame maps to feature-not-implemented.
# Fixture is derived from vp80-04-partitions-1404.webp by clearing show_frame
# bit in frame tag at payload offset 0.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_vp8_hidden_frame.webp"
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
    echo "not ok" 1 - "forced builtin loader corrupted VP8 hidden frame fixture unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 hidden frame fixture missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=1|kind=ERR|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 hidden frame fixture malformed error contract header"
    exit 0
}

test "${diag_line#*W_UNSUP_VP8_FEATURE*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 hidden frame fixture missing W_UNSUP_VP8_FEATURE contract code"
    exit 0
}

echo "ok" 1 - "forced builtin loader corrupted VP8 hidden frame fixture emits W_UNSUP_VP8_FEATURE contract code"
exit 0
