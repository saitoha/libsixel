#!/bin/sh
# Verify opt-in keycolor mode changes RGBA16 PNG output in libpng loader path.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png"
out_default="${ARTIFACT_LOCAL_DIR}/libpng-trns-keycolor-rgba16-default.six"
out_optin="${ARTIFACT_LOCAL_DIR}/libpng-trns-keycolor-rgba16-optin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - libpng rgba16 default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - libpng rgba16 opt-in render failed"
    exit 0
}

cmp -s "${out_default}" "${out_optin}" && {
    echo "not ok 1 - opt-in unexpectedly ignored for libpng rgba16 PNG"
    exit 0
}

    echo "ok 1 - opt-in changes libpng rgba16 PNG output"


exit 0
