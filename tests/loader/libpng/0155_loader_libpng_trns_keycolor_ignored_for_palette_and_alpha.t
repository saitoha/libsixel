#!/bin/sh
# Verify opt-in is ignored for a palette+tRNS PNG sample.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out_default="${ARTIFACT_LOCAL_DIR}/trns-keycolor-palette-default-tbbn3p08.six"
out_optin="${ARTIFACT_LOCAL_DIR}/trns-keycolor-palette-optin-tbbn3p08.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - palette default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - palette opt-in render failed"
    exit 0
}

cmp -s "${out_default}" "${out_optin}" || {
    echo "not ok 1 - opt-in unexpectedly changes palette+tRNS output"
    exit 0
}

    echo "ok 1 - opt-in is ignored for palette+tRNS"


exit 0
