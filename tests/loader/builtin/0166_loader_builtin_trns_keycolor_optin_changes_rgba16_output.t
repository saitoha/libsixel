#!/bin/sh
# Verify builtin RGBA16 PNG keycolor is enabled by default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-rgba16-default.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-rgba16-env0.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin rgba16 default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin rgba16 SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 render failed"
    exit 0
}

cmp -s "${out_default}" "${out_off}" && {
    echo "not ok 1 - builtin rgba16 default keycolor is unexpectedly disabled"
    exit 0
}

echo "ok 1 - builtin rgba16 keycolor is enabled by default"

exit 0
