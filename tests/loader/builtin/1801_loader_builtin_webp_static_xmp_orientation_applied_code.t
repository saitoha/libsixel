#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test confirming static WebP applies XMP orientation fallback.
# Derived fixture: orientation_exif_o6_vp8_static_12x8.webp
# Replaced EXIF chunk with XMP chunk: tiff:Orientation="6".

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_xmp_o6_vp8_static_12x8.webp"
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
    echo "not ok" 1 - "builtin static XMP orientation fallback decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
case "${trace_output}" in *LSXWEBP1\|*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin static XMP orientation fallback missing LSXWEBP1 header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

case "${diag_line}" in LSXWEBP1\|rc=0\|kind=OK\|codes=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin static XMP orientation fallback malformed success header"
    exit 0
}

case "${diag_line}" in *W_OK_VP8_STATIC**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin static XMP orientation fallback missing W_OK_VP8_STATIC"
    exit 0
}

case "${diag_line}" in *W_META_XMP_APPLIED**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "builtin static XMP orientation fallback missing W_META_XMP_APPLIED"
    exit 0
}

case "${diag_line}" in *W_META_XMP_IGNORED**) false ;; *) true ;; esac || {
    echo "not ok" 1 - "builtin static XMP orientation fallback unexpectedly emitted W_META_XMP_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin static XMP orientation fallback emits APPLIED contract"
exit 0
