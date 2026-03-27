#!/bin/sh
# Verify builtin loader enables tRNS keycolor by default for palette PNG.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_default.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_env0.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok" 1 - "builtin palette+tRNS default decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok" 1 - "builtin palette+tRNS SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 decode failed"
    exit 0
}

cmp -s "${out_default}" "${out_off}" && {
    echo "not ok" 1 - "builtin default palette+tRNS keycolor mode is unexpectedly disabled"
    exit 0
}

echo "ok" 1 - "builtin default enables palette+tRNS keycolor mode"

exit 0
