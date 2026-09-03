#!/bin/sh
# Verify invalid APNG keycolor input uses the builtin enabled default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_invalid="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env2.six"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
              -Lbuiltin! \
              -d fs:scan=raster \
              "${input_png}" >"${out_invalid}" || {
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin! \
              -d fs:scan=raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG keycolor default render failed"
    exit 0
}

cmp -s "${out_invalid}" "${out_default}" || {
    echo "not ok 1 - builtin APNG invalid keycolor value did not use default"
    exit 0
}

echo "ok 1 - builtin APNG invalid keycolor value uses the enabled default"

exit 0
