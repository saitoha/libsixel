#!/bin/sh
# TAP test confirming animated WebP keeps ICCP precedence over prefixed XMP CMS.
# Derived fixture: animated-lossy-8x8-2frame-min-embedded-srgb-icc.webp
# Replaced XMP property name: photoshop:ICCProfile -> myphotosh:ICCProfile.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-embedded-srgb-icc-xmp-icc-displayp3-psprefix.webp"
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
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin animated ICCP precedence decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin animated ICCP precedence missing LSXWEBP1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated ICCP precedence missing W_OK_ANIM"
    exit 0
}

test "${diag_line#*W_META_ICCP_APPLIED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated ICCP precedence missing W_META_ICCP_APPLIED"
    exit 0
}

test "${diag_line#*W_META_XMP_CMS_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated ICCP precedence missing W_META_XMP_CMS_IGNORED"
    exit 0
}

test "${diag_line#*W_META_XMP_CMS_APPLIED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin animated ICCP precedence unexpectedly emitted W_META_XMP_CMS_APPLIED"
    exit 0
}

echo "ok" 1 - "builtin animated ICCP precedence keeps XMP CMS ignored"
exit 0
