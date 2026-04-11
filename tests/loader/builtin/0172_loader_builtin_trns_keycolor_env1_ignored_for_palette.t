#!/bin/sh
# Verify SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 is ignored for palette+tRNS in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_default_tbbn3p08.six"
out_env1="${ARTIFACT_LOCAL_DIR}/builtin_trns_keycolor_palette_env1_tbbn3p08.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin palette+tRNS default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_env1}" || {
    echo "not ok 1 - builtin palette+tRNS SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 render failed"
    exit 0
}

cmp -s "${out_default}" "${out_env1}" || {
    echo "not ok 1 - builtin SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 unexpectedly changes palette+tRNS output"
    exit 0
}

echo "ok 1 - builtin SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 is ignored for palette+tRNS output"

exit 0
