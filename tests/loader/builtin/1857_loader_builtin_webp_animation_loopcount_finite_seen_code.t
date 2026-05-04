#!/bin/sh
# TAP test confirming finite ANIM loopcount emits finite timing contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
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
    echo "not ok" 1 - "builtin finite-loop animation decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin finite-loop animation missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin finite-loop animation malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin finite-loop animation missing W_OK_ANIM"
    exit 0
}

test "${diag_line#*W_ANIM_LOOPCOUNT_FINITE_SEEN*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin finite-loop animation missing W_ANIM_LOOPCOUNT_FINITE_SEEN"
    exit 0
}

test "${diag_line#*W_ANIM_LOOPCOUNT_INFINITE_SEEN*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin finite-loop animation unexpectedly emitted W_ANIM_LOOPCOUNT_INFINITE_SEEN"
    exit 0
}

echo "ok" 1 - "builtin finite-loop animation emits only finite loopcount contract"
exit 0
