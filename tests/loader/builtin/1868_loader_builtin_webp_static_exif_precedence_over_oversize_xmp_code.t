#!/bin/sh
# TAP test confirming EXIF precedence over oversize XMP orientation metadata.
# Derived fixture: orientation_exif_o6_vp8_static_12x8_xmp_o3.webp
# Replaced XMP payload with 262145-byte orientation metadata.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_vp8_static_12x8_xmp_o3_oversize.webp"
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
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP missing LSXWEBP1 header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP malformed success header"
    exit 0
}

test "${diag_line#*W_META_EXIF_APPLIED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP missing W_META_EXIF_APPLIED"
    exit 0
}

test "${diag_line#*W_META_XMP_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP missing W_META_XMP_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_SIZE_LIMIT_IGNORED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF precedence over oversize XMP unexpectedly emitted W_META_XMP_SIZE_LIMIT_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin static EXIF precedence suppresses XMP size-limit diagnostics"
exit 0
