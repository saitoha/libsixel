#!/bin/sh
# TAP test confirming non-integer animation start frame value keeps error contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "builtin WebP animation start frame non-integer unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin WebP animation start frame non-integer missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation start frame non-integer malformed error contract header"
    exit 0
}

test "${diag_line#*WEBP_ERR*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation start frame non-integer missing WEBP_ERR contract code"
    exit 0
}

echo "ok" 1 - "builtin WebP animation start frame non-integer emits WEBP_ERR"
exit 0
