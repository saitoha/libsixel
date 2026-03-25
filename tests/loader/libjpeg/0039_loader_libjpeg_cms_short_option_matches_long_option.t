#!/bin/sh
# Verify libjpeg loader suboption short form e=auto matches
# cms_engine=auto for JPEG ICC.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
output_long="${ARTIFACT_LOCAL_DIR}/libjpeg_cms_long_option.six"
output_short="${ARTIFACT_LOCAL_DIR}/libjpeg_cms_short_option.six"

run_img2sixel -L libjpeg:cms_engine=auto! "${input_jpeg}" >"${output_long}" || {
    echo "not ok" 1 - "libjpeg decode with cms_engine=auto failed"
    exit 0
}

run_img2sixel -L libjpeg:e=auto! "${input_jpeg}" >"${output_short}" || {
    echo "not ok" 1 - "libjpeg decode with e=auto failed"
    exit 0
}

cmp -s "${output_long}" "${output_short}" || {
    echo "not ok" 1 - "libjpeg e=auto output differs from cms_engine=auto output"
    exit 0
}

echo "ok" 1 - "libjpeg e=auto output matches cms_engine=auto output"
exit 0
