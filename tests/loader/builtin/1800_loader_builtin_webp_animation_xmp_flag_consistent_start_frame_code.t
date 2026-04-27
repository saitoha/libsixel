#!/bin/sh
# TAP test confirming animated XMP-consistent stream keeps W_OK_ANIM with start_frame.
# Derived fixture: animated-lossy-8x8-2frame-min.webp
# Patched offsets: RIFF size at 0x04, VP8X flags at 0x14, appended XMP chunk.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-xmp.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_ANIMATION_START_FRAME_NO=1
export SIXEL_LOADER_ANIMATION_START_FRAME_NO
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin animated XMP fixture start_frame decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin animated XMP fixture start_frame missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=0\|kind=OK\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP fixture start_frame malformed success contract header"
    exit 0
}

test "${diag_line#*W_OK_ANIM*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin animated XMP fixture start_frame missing W_OK_ANIM"
    exit 0
}

echo "ok" 1 - "builtin animated XMP fixture keeps W_OK_ANIM with start_frame"
exit 0
