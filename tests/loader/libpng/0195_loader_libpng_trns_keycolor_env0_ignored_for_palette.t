#!/bin/sh
# Verify env=0 is ignored for palette+tRNS in libpng loader path.

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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out_default="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_palette_default.six"
out_off="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_palette_env0.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok" 1 - "libpng palette+tRNS default decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok" 1 - "libpng palette+tRNS env=0 decode failed"
    exit 0
}

if cmp -s "${out_default}" "${out_off}"; then
    echo "ok" 1 - "libpng env=0 is ignored for palette+tRNS output"
else
    echo "not ok" 1 - "libpng env=0 unexpectedly changes palette+tRNS output"
fi

exit 0
