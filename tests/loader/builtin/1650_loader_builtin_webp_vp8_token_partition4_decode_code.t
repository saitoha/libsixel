#!/bin/sh
# TAP test confirming forced builtin loader decodes VP8 token partition=4.
# Fixture source: webmproject/libwebp-test-data@06ddd96e276c2c638a72d39d3c0f340afd61978c
# Fixture SHA256: 8d2daf0d7c7e902208621342450bd4009a7bfe3b6aaf36b7d43d232066cd9037

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/vp80-04-partitions-1405.webp"
trace_output=''
diag_line=''
ctrl_line=''
command_status=0
nl='
'

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=webp_decode \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 decode failed"
    exit 0
}

ctrl_line=${trace_output#*LSXWEBPDBG|diag=VP8CTRL}
test "${ctrl_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 missing VP8CTRL diagnostics"
    exit 0
}

ctrl_line="LSXWEBPDBG|diag=VP8CTRL${ctrl_line}"
ctrl_line=${ctrl_line%%"${nl}"*}

test "${ctrl_line#*|tok=4|}" != "${ctrl_line}" || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 missing tok=4 diagnostics"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1|rc=0|kind=OK|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_VP8_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader decodes VP8 token partition=4 missing W_OK_VP8_STATIC contract code"
    exit 0
}

echo "ok" 1 - "forced builtin loader decodes VP8 token partition=4 with stable contract codes"
exit 0
