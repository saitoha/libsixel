#!/bin/sh
# Verify builtin palette+tRNS keycolor path keeps libpng-equivalent tone.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tm3n3p02.png"
out_builtin="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_tm3n3p02.six"
out_libpng="${ARTIFACT_LOCAL_DIR}/libpng_trns_keycolor_tm3n3p02.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Lbuiltin:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_builtin}" || {
    echo "not ok" 1 - "builtin palette+tRNS keycolor decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Llibpng:cms_engine=none! \
              -d none -y raster \
              "${input_png}" >"${out_libpng}" || {
    echo "not ok" 1 - "libpng palette+tRNS keycolor decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:1.0" \
         "${out_builtin}" "${out_libpng}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "builtin palette+tRNS keycolor tone differs from libpng"
    exit 0
}

echo "ok" 1 - "builtin palette+tRNS keycolor tone matches libpng"

exit 0
