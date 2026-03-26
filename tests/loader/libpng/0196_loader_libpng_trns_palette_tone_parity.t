#!/bin/sh
# Verify libpng palette+tRNS keycolor tone stays in parity with builtin.

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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tm3n3p02.png"
out_libpng="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_tm3n3p02.six"
out_builtin="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_tm3n3p02.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Llibpng:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_libpng}" || {
    echo "not ok" 1 - "libpng palette+tRNS keycolor decode failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Lbuiltin:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_builtin}" || {
    echo "not ok" 1 - "builtin palette+tRNS keycolor decode failed"
    exit 0
}

if cmp -s "${out_libpng}" "${out_builtin}"; then
    echo "ok" 1 - "libpng palette+tRNS keycolor tone matches builtin"
else
    echo "not ok" 1 - "libpng palette+tRNS keycolor tone differs from builtin"
fi

exit 0
