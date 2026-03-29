#!/bin/sh
# Verify libtiff loader suboption short form e=auto matches
# cms_engine=auto for TIFF.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff"
output_long="${ARTIFACT_LOCAL_DIR}/libtiff_cms_long_option.six"
output_short="${ARTIFACT_LOCAL_DIR}/libtiff_cms_short_option.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libtiff:cms_engine=auto! "${input_tiff}" >"${output_long}" || {
    echo "not ok" 1 - "libtiff decode with cms_engine=auto failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libtiff:e=auto! "${input_tiff}" >"${output_short}" || {
    echo "not ok" 1 - "libtiff decode with e=auto failed"
    exit 0
}

cmp -s "${output_long}" "${output_short}" || {
    echo "not ok" 1 - "libtiff e=auto output differs from cms_engine=auto output"
    exit 0
}

echo "ok" 1 - "libtiff e=auto output matches cms_engine=auto output"
exit 0
