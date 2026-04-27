#!/bin/sh
# TAP test confirming invalid ICC animation keeps ICCP ignored with CMS auto.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_invalid_icc_anim2.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_CMS_ENGINE=auto
export SIXEL_LOADER_BUILTIN_CMS_ENGINE
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp decode failed with CMS auto"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp missing LSXWEBP1 contract header with CMS auto"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp malformed success contract header with CMS auto"
    exit 0
}

test "${diag_line#*W_OK_VP8L_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp missing W_OK_VP8L_STATIC with CMS auto"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp missing W_OK_ANIM with CMS auto"
    exit 0
}

test "${diag_line#*W_META_ICCP_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp missing W_META_ICCP_IGNORED with CMS auto"
    exit 0
}

echo "ok" 1 - "builtin palette_lossless_embedded_invalid_icc_anim2.webp keeps ICCP ignored with CMS auto"
exit 0
