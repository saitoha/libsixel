#!/bin/sh
# TAP test confirming static XMP-consistent WebP succeeds with orientation off.
# Derived fixture: orientation_exif_o6_vp8_static_12x8.webp
# Patched offsets: RIFF size at 0x04, VP8X flags at 0x14, appended XMP chunk.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_vp8_static_12x8_xmp.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin static XMP fixture decode failed with orientation=off"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static XMP fixture missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP fixture malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_VP8_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP fixture missing W_OK_VP8_STATIC"
    exit 0
}

test "${diag_line#*W_META_EXIF_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP fixture missing W_META_EXIF_IGNORED"
    exit 0
}

test "${diag_line#*W_ERR_VP8X_FLAG_XMP_MISMATCH*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP fixture unexpectedly emitted W_ERR_VP8X_FLAG_XMP_MISMATCH"
    exit 0
}

echo "ok" 1 - "builtin static XMP fixture keeps success codes without XMP mismatch"
exit 0
