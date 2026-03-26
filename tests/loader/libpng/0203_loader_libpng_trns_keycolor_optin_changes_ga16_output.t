#!/bin/sh
# Verify opt-in keycolor mode changes GA16 PNG output in libpng loader path.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png"
out_default="${ARTIFACT_LOCAL_DIR}/libpng-trns-keycolor-ga16-default.six"
out_optin="${ARTIFACT_LOCAL_DIR}/libpng-trns-keycolor-ga16-optin.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - libpng ga16 default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - libpng ga16 opt-in render failed"
    exit 0
}

if cmp -s "${out_default}" "${out_optin}"; then
    echo "not ok 1 - opt-in unexpectedly ignored for libpng ga16 PNG"
else
    echo "ok 1 - opt-in changes libpng ga16 PNG output"
fi

exit 0
