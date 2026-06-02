#!/bin/sh
# TAP test confirming oversize EXIF is ignored non-fatally on static WebP.
# Derived fixture: orientation_exif_o6_vp8_static_12x8.webp
# Replaced EXIF payload size to 1048577 bytes and stored as .webp.gz.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -n "${SIXEL_TEST_GZIP-}" || {
    printf "1..0 # SKIP gzip is unavailable for compressed WebP fixture\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_BUILDDIR}/tests/data/inputs/formats/orientation_exif_o6_vp8_static_12x8_exif_overlimit.webp"
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
    echo "not ok" 1 - "builtin static EXIF overlimit decode failed"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "builtin static EXIF overlimit missing LSXWEBP1"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#*W_OK_VP8_STATIC*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF overlimit missing W_OK_VP8_STATIC"
    exit 0
}

test "${diag_line#*W_META_EXIF_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF overlimit missing W_META_EXIF_IGNORED"
    exit 0
}

test "${diag_line#*W_META_EXIF_SIZE_LIMIT_IGNORED*}" != "${diag_line}" || {
    echo "not ok" 1 - "builtin static EXIF overlimit missing W_META_EXIF_SIZE_LIMIT_IGNORED"
    exit 0
}

echo "ok" 1 - "builtin static EXIF overlimit emits size-limit ignored contract"
exit 0
