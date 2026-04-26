#!/bin/sh
# TAP test confirming short VP8 ALPH compression=1 payload maps to stream error.
# Fixture is derived from webp-vp8-alpha-snake64-compression1.webp by removing
# bytes from the ALPH payload and adjusting RIFF/chunk size fields.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_vp8_alpha_compression1_payload_short.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

trace_output=$(set +xv; \
    SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "forced builtin loader short VP8 ALPH compression=1 payload unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader short VP8 ALPH compression=1 payload missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=1|kind=ERR|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader short VP8 ALPH compression=1 payload malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8_STREAM*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader short VP8 ALPH compression=1 payload missing W_ERR_VP8_STREAM"
    exit 0
}

echo "ok" 1 - "forced builtin loader short VP8 ALPH compression=1 payload emits W_ERR_VP8_STREAM"
exit 0
