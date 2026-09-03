#!/bin/sh
# Verify an invalid PNG keycolor boolean falls back to the enabled default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_invalid="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-env2-tbbn0g04.six"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-env1-tbbn0g04.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_invalid}" || {
    echo "not ok 1 - builtin SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin keycolor default render failed"
    exit 0
}

cmp -s "${out_invalid}" "${out_default}" || {
    echo "not ok 1 - builtin invalid keycolor value did not use default"
    exit 0
}

echo "ok 1 - builtin invalid keycolor value uses the enabled default"

exit 0
