#!/bin/sh
# TAP test confirming ANIM duration=0 streams expose timing contract codes.
# Fixture origin: animated-lossless-8x8-2frame-min.webp.
# Patch summary: first ANMF duration bytes were rewritten from 10 to 0.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min-duration0.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin duration=0 animation decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin duration=0 animation missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin duration=0 animation malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin duration=0 animation missing W_OK_ANIM"
    exit 0
}

test "${diag_line#*W_ANIM_DURATION_ZERO_SEEN*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin duration=0 animation missing W_ANIM_DURATION_ZERO_SEEN"
    exit 0
}

test "${diag_line#*W_ANIM_DURATION_NONZERO_SEEN*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin duration=0 animation missing W_ANIM_DURATION_NONZERO_SEEN"
    exit 0
}

echo "ok" 1 - "builtin duration=0 animation emits timing duration contract codes"
exit 0
