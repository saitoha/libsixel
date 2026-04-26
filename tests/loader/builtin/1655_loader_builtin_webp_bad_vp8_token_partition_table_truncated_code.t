#!/bin/sh
# TAP test confirming VP8 token partition table truncation maps to stream error.
# Fixture is derived from vp80-04-partitions-1406.webp by rewriting frame tag
# bytes at payload offset 0 to increase first partition size.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_vp8_token_partition_table_truncated.webp"
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
    echo "not ok" 1 - "forced builtin loader corrupted VP8 token table truncation fixture unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 token table truncation fixture missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=1|kind=ERR|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 token table truncation fixture malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8_STREAM*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader corrupted VP8 token table truncation fixture missing W_ERR_VP8_STREAM contract code"
    exit 0
}

echo "ok" 1 - "forced builtin loader corrupted VP8 token table truncation fixture emits W_ERR_VP8_STREAM contract code"
exit 0
