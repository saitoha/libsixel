#!/bin/sh
# TAP test confirming alpha animation background decodes on builtin path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-bg112233-a80.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=webp_decode \
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin WebP alpha animation decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin WebP alpha animation missing header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=0|kind=OK|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP alpha animation malformed header"
    exit 0
}
test "${diag_line#*W_OK_VP8L_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP alpha animation missing VP8L OK code"
    exit 0
}
test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP alpha animation missing ANIM OK code"
    exit 0
}

echo "ok" 1 - "builtin WebP alpha animation keeps VP8L+ANIM OK contract"
exit 0
