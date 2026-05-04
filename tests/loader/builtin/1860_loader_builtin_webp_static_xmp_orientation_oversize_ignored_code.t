#!/bin/sh
# TAP test confirming oversize static XMP orientation is ignored non-fatally.
# Derived fixture: orientation_xmp_o6_vp8_static_12x8.webp
# Replaced XMP payload with 262145-byte orientation metadata.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_xmp_o6_vp8_static_12x8_xmp_oversize.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_ORIENTATION=on
export SIXEL_LOADER_BUILTIN_ORIENTATION
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin static XMP orientation oversize decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize missing LSXWEBP1 header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize malformed success header"
    exit 0
}

test "${diag_line#*W_OK_VP8_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize missing W_OK_VP8_STATIC"
    exit 0
}

test "${diag_line#*W_META_XMP_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize missing W_META_XMP_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_SIZE_LIMIT_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize missing W_META_XMP_SIZE_LIMIT_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_APPLIED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP orientation oversize unexpectedly emitted W_META_XMP_APPLIED"
    exit 0
}

echo "ok" 1 - "builtin static XMP orientation oversize emits size-limit ignored contract"
exit 0
