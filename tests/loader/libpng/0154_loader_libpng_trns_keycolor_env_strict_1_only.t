#!/bin/sh
# Verify env value 2 is ignored and matches explicit opt-out output.

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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_invalid="${ARTIFACT_LOCAL_DIR}/trns-keycolor-env2-tbbn0g04.six"
out_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-env0-tbbn0g04.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster \
              "${input_png}" >"${out_invalid}" || {
    echo "not ok 1 - env=2 render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - env=0 render failed"
    exit 0
}

if cmp -s "${out_invalid}" "${out_off}"; then
    echo "ok 1 - env=2 is ignored and stays opt-out"
else
    echo "not ok 1 - env=2 unexpectedly changes output"
fi

exit 0
