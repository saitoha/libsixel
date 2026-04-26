#!/bin/sh
# TAP test confirming builtin WebP animation supports subrect blend/dispose.
# Fixture source frames:
#   tests/data/inputs/snake_64.webp
#   tests/data/inputs/formats/webp-vp8-alpha-snake64-filter1.webp
#   tests/data/inputs/snake_64_embedded_a98_icc.webp
# Mux command:
#   webpmux -frame f0.webp +80+0+0+0-b \
#           -frame f1.webp +80+16+0+1+b \
#           -frame f2.webp +80+16+0+0-b \
#           -loop 0 -bgcolor 255,0,0,0 -o animated-lossy-alpha-subrect-80x64-3frame-min.webp

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-alpha-subrect-80x64-3frame-min.webp"
trace_output=''
diag_line=''
command_status=0
nl='\
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin WebP animation subrect blend/dispose decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin WebP animation subrect blend/dispose missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=0|kind=OK|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation subrect blend/dispose malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin WebP animation subrect blend/dispose missing W_OK_ANIM contract code"
    exit 0
}

echo "ok" 1 - "builtin WebP animation subrect blend/dispose emits W_OK_ANIM contract code"
exit 0
