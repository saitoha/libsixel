#!/bin/sh
# Verify opt-in is ignored for alpha-channel PNG sample.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png"
out_default="${ARTIFACT_LOCAL_DIR}/trns-keycolor-alpha-default-basn4a08.six"
out_optin="${ARTIFACT_LOCAL_DIR}/trns-keycolor-alpha-optin-basn4a08.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - alpha default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - alpha opt-in render failed"
    exit 0
}

if cmp -s "${out_default}" "${out_optin}"; then
    echo "ok 1 - opt-in is ignored for alpha PNG"
else
    echo "not ok 1 - opt-in unexpectedly changes alpha PNG output"
fi

exit 0
