#!/bin/sh
# Verify empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR matches default behavior in libpng APNG path.

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

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_empty="${ARTIFACT_LOCAL_DIR}/apng-trns-keycolor-env-empty.six"
out_default="${ARTIFACT_LOCAL_DIR}/apng-trns-keycolor-default.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_empty}" || {
    echo "not ok 1 - libpng APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - libpng APNG default render failed"
    exit 0
}

cmp -s "${out_empty}" "${out_default}" || {
    echo "not ok 1 - libpng APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR unexpectedly differs from default"
    exit 0
}

    echo "ok 1 - libpng APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR matches default"


exit 0
