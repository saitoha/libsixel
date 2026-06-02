#!/bin/sh
# TAP test confirming oversize ICCP is ignored without size-limit code when CMS is off.
# Derived fixture: snake_64_embedded_a98_icc_xmp_iccprofile_unprefixed_display_dash_p3.webp
# Replaced ICCP payload size to 1048577 bytes and stored as .webp.gz.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -n "${SIXEL_TEST_GZIP-}" || {
    printf "1..0 # SKIP gzip is unavailable for compressed WebP fixture\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_BUILDDIR}/tests/data/inputs/formats/snake_64_embedded_a98_icc_xmp_iccprofile_iccp_overlimit.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_CMS_ENGINE=none
export SIXEL_LOADER_BUILTIN_CMS_ENGINE
SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin static ICCP overlimit cms=none decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static ICCP overlimit cms=none missing LSXWEBP1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*W_META_ICCP_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static ICCP overlimit cms=none missing W_META_ICCP_IGNORED"
    exit 0
}

test "${diag_line#*W_META_ICCP_SIZE_LIMIT_IGNORED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin static ICCP overlimit cms=none unexpectedly emitted W_META_ICCP_SIZE_LIMIT_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin static ICCP overlimit cms=none keeps size-limit code suppressed"
exit 0
