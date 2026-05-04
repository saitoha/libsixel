#!/bin/sh
# TAP test confirming exact 1MiB ICCP payload stays accepted.
# Derived fixture: webp-static-icc-overlimit-padded.webp
# Replaced ICCP payload size from 1049601 to exactly 1048576 bytes.

set -eux

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_BUILDDIR}/tests/data/inputs/formats/webp-static-icc-limit-padded.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_CMS_ENGINE=auto
export SIXEL_LOADER_BUILTIN_CMS_ENGINE
SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin static exact-limit ICCP decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static exact-limit ICCP missing LSXWEBP1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*W_META_ICCP_SIZE_LIMIT_IGNORED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin static exact-limit ICCP unexpectedly emitted W_META_ICCP_SIZE_LIMIT_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin static exact-limit ICCP is accepted without size-limit diagnostics"
exit 0
