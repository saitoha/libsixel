#!/bin/sh
# TAP test confirming animated WebP accepts element-form XMP orientation syntax.
# Derived fixture: animated-lossy-8x8-2frame-min-xmp-o6.webp
# Replaced XMP payload with: <tiff:Orientation>6</tiff:Orientation>.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-xmp-o6-elem.webp"
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
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin animated XMP element orientation decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin animated XMP element orientation missing LSXWEBP1 header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP element orientation malformed success header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP element orientation missing W_OK_ANIM"
    exit 0
}

test "${diag_line#*W_META_XMP_APPLIED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP element orientation missing W_META_XMP_APPLIED"
    exit 0
}

test "${diag_line#*W_META_XMP_IGNORED*}" = "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP element orientation unexpectedly emitted W_META_XMP_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin animated XMP element orientation emits APPLIED contract"
exit 0
