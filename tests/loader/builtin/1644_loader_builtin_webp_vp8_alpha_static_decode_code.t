#!/bin/sh
# TAP test confirming forced builtin loader decodes static VP8+ALPH WebP.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "forced builtin loader decodes static VP8+ALPH WebP decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader decodes static VP8+ALPH WebP missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader decodes static VP8+ALPH WebP malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_VP8_ALPHA_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader decodes static VP8+ALPH WebP missing W_OK_VP8_ALPHA_STATIC contract code"
    exit 0
}

echo "ok" 1 - "forced builtin loader decodes static VP8+ALPH WebP emits W_OK_VP8_ALPHA_STATIC contract code"
exit 0
