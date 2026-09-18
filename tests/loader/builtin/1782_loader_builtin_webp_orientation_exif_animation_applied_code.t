#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming EXIF orientation applies on animated VP8L stream.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_exif_o6_anim_12x8.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
SIXEL_LOADER_BUILTIN_ORIENTATION=1
export SIXEL_LOADER_BUILTIN_ORIENTATION
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -S -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp decode failed with orientation=1"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
case "${trace_output}" in *LSXWEBP1\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp missing LSXWEBP1 contract header with orientation=1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in LSXWEBP1\|rc=0\|kind=OK\|codes=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp malformed success contract header with orientation=1"
    exit 0
}

case "${diag_line}" in *W_OK_ANIM**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp missing W_OK_ANIM with orientation=1"
    exit 0
}

case "${diag_line}" in *W_META_EXIF_APPLIED**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp missing W_META_EXIF_APPLIED with orientation=1"
    exit 0
}

echo "ok" 1 - "builtin orientation_exif_o6_anim_12x8.webp emits W_META_EXIF_APPLIED with orientation=1"
exit 0
