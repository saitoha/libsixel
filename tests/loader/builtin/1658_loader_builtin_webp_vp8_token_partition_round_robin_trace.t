#!/bin/sh
# TAP test confirming VP8 token partition routing rotates per macroblock row.
# Fixture source: webmproject/libwebp-test-data@06ddd96e276c2c638a72d39d3c0f340afd61978c
# Fixture SHA256: 25cd4540f189f61ab0119f8f26e3dc28ba1a7840843b205389948dc3019eee6d

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/vp80-04-partitions-1404.webp"
trace_output=''
ctrl_line=''
probe=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace decode failed"
    exit 0
}

ctrl_line=${trace_output#*LSXWEBPDBG\|diag=VP8CTRL}
test "${ctrl_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace missing VP8CTRL diagnostics"
    exit 0
}

ctrl_line="LSXWEBPDBG|diag=VP8CTRL${ctrl_line}"
ctrl_line=${ctrl_line%%"${nl}"*}

test "${ctrl_line#*\|tok=2\|}" != "${ctrl_line}" || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace missing tok=2 diagnostics"
    exit 0
}

probe=${trace_output#*LSXWEBPDBG\|diag=VP8MB\|x=0\|y=0\|part=0\|}
test "${probe}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace missing row0 partition0 diagnostics"
    exit 0
}

probe=${trace_output#*LSXWEBPDBG\|diag=VP8MB\|x=0\|y=1\|part=1\|}
test "${probe}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace missing row1 partition1 diagnostics"
    exit 0
}

probe=${trace_output#*LSXWEBPDBG\|diag=VP8MB\|x=0\|y=1\|part=0\|}
test "${probe}" = "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader VP8 token partition round robin trace emitted unexpected row1 partition0 diagnostics"
    exit 0
}

echo "ok" 1 - "forced builtin loader VP8 token partition round robin trace is stable"
exit 0
