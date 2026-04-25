#!/bin/sh
# TAP test confirming builtin loader decodes VP8+ALPHA with filter=2.
# Fixture is derived from webp-vp8-alpha-snake64-alpha00.webp.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-vp8-alpha-snake64-filter2.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=webp_decode \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA filter=2 decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA filter=2 missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=0|kind=OK|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA filter=2 malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_VP8_ALPHA_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA filter=2 missing W_OK_VP8_ALPHA_STATIC"
    exit 0
}

echo "ok" 1 - "forced builtin loader VP8+ALPHA filter=2 emits W_OK_VP8_ALPHA_STATIC"
exit 0
