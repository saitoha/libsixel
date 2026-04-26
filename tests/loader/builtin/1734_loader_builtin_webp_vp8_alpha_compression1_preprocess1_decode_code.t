#!/bin/sh
# TAP test confirming VP8+ALPHA compression=1 preprocess=1 decodes successfully.
# Fixture is derived from webp-vp8-alpha-snake64-compression1.webp by setting
# ALPH control byte at offset 38 to 0x11.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/webp-vp8-alpha-snake64-compression1-preprocess1.webp"
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
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA compression=1 preprocess=1 decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA compression=1 preprocess=1 missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA compression=1 preprocess=1 malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_VP8_ALPHA_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader VP8+ALPHA compression=1 preprocess=1 missing W_OK_VP8_ALPHA_STATIC"
    exit 0
}

echo "ok" 1 - "forced builtin loader VP8+ALPHA compression=1 preprocess=1 emits W_OK_VP8_ALPHA_STATIC"
exit 0
