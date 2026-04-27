#!/bin/sh
# TAP test confirming animated WebP keeps EXIF priority over XMP with orientation off.
# Derived fixture: animated-lossy-8x8-2frame-min-exif-o6.webp
# Patched offsets: VP8X flags at 0x14 to include XMP, appended XMP orientation=3.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-exif-o6-xmp-o3.webp"
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
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off missing LSXWEBP1 header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off malformed success header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off missing W_OK_ANIM"
    exit 0
}

test "${diag_line#*W_META_EXIF_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off missing W_META_EXIF_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off missing W_META_XMP_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_APPLIED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin animated EXIF+XMP orientation off unexpectedly emitted W_META_XMP_APPLIED"
    exit 0
}

echo "ok" 1 - "builtin animated EXIF+XMP keeps EXIF priority with orientation off"
exit 0
