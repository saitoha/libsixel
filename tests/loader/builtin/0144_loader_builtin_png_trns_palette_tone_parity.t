#!/bin/sh
# Verify builtin palette+tRNS keycolor path keeps libpng-equivalent tone.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tm3n3p02.png"
out_builtin="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_tm3n3p02.six"
out_libpng="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_tm3n3p02.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Lbuiltin:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_builtin}" || {
    echo "not ok" 1 - "builtin palette+tRNS keycolor decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Llibpng:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_libpng}" || {
    echo "not ok" 1 - "libpng palette+tRNS keycolor decode failed"
    exit 0
}

if cmp -s "${out_builtin}" "${out_libpng}"; then
    echo "ok" 1 - "builtin palette+tRNS keycolor tone matches libpng"
else
    echo "not ok" 1 - "builtin palette+tRNS keycolor tone differs from libpng"
fi

exit 0
