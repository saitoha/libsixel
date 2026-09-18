#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming ANIM duration>0 streams expose nonzero duration contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
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
    echo "not ok" 1 - "builtin duration>0 animation decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
case "${trace_output}" in *LSXWEBP1\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin duration>0 animation missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in LSXWEBP1\|rc=0\|kind=OK\|codes=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin duration>0 animation malformed success contract header"
    exit 0
}

case "${diag_line}" in *W_OK_ANIM**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin duration>0 animation missing W_OK_ANIM"
    exit 0
}

case "${diag_line}" in *W_ANIM_DURATION_NONZERO_SEEN**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin duration>0 animation missing W_ANIM_DURATION_NONZERO_SEEN"
    exit 0
}

case "${diag_line}" in *W_ANIM_DURATION_ZERO_SEEN**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "builtin duration>0 animation unexpectedly emitted W_ANIM_DURATION_ZERO_SEEN"
    exit 0
}

echo "ok" 1 - "builtin duration>0 animation emits only nonzero duration contract"
exit 0
