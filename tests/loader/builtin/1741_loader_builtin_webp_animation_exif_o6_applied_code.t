#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming ANIM path applies EXIF orientation on o6 fixture.
# Fixture is derived from animated-lossy-8x8-2frame-min.webp by injecting
# EXIF metadata extracted from orientation_exif_o6_vp8_static_12x8.webp.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min-exif-o6.webp"
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
    -L builtin! -ldisable -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin WebP animation EXIF o6 applied contract decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
case "${trace_output}" in *LSXWEBP1\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin WebP animation EXIF o6 applied contract missing header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in LSXWEBP1\|rc=0\|kind=OK\|codes=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin WebP animation EXIF o6 applied contract malformed header"
    exit 0
}

case "${diag_line}" in *W_OK_ANIM**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin WebP animation EXIF o6 applied contract missing ANIM OK code"
    exit 0
}

case "${diag_line}" in *W_META_EXIF_APPLIED**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin WebP animation EXIF o6 applied contract missing EXIF APPLIED code"
    exit 0
}

echo "ok" 1 - "builtin WebP animation EXIF o6 applied contract keeps APPLIED code"
exit 0
