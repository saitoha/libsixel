#!/bin/sh
# Verify libtiff loader suboption short form c=1 matches cms=1 for TIFF.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff"
output_long="${ARTIFACT_LOCAL_DIR}/libtiff_cms_long_option.six"
output_short="${ARTIFACT_LOCAL_DIR}/libtiff_cms_short_option.six"

run_img2sixel -L libtiff:cms=1! "${input_tiff}" >"${output_long}" || {
    echo "not ok" 1 - "libtiff decode with cms=1 failed"
    exit 0
}

run_img2sixel -L libtiff:c=1! "${input_tiff}" >"${output_short}" || {
    echo "not ok" 1 - "libtiff decode with c=1 failed"
    exit 0
}

cmp -s "${output_long}" "${output_short}" || {
    echo "not ok" 1 - "libtiff c=1 output differs from cms=1 output"
    exit 0
}

echo "ok" 1 - "libtiff c=1 output matches cms=1 output"
exit 0
