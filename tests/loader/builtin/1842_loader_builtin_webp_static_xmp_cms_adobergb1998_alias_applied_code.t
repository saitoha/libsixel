#!/bin/sh
# TAP test confirming static WebP applies XMP CMS alias name for AdobeRGB1998.
# Derived fixture: orientation_plain_12x8.webp
# Added XMP payload: photoshop:ICCProfile="AdobeRGB1998".

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_xmp_icc_adobergb1998_alias_vp8_static_12x8.webp"
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
    echo "not ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias missing LSXWEBP1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias malformed success header"
    exit 0
}

test "${diag_line#*W_OK_VP8L_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias missing W_OK_VP8L_STATIC"
    exit 0
}

test "${diag_line#*W_META_XMP_CMS_APPLIED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias missing W_META_XMP_CMS_APPLIED"
    exit 0
}

echo "ok" 1 - "builtin static XMP CMS AdobeRGB1998 alias emits APPLIED contract"
exit 0
