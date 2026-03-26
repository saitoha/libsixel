#!/bin/sh
# Verify builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 matches default output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default-vs-env1-default.six"
out_env1="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default-vs-env1-env1.six"

run_img2sixel -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_env1}" || {
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 render failed"
    exit 0
}

if cmp -s "${out_default}" "${out_env1}"; then
    echo "ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 matches default output"
else
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 differs from default output"
fi

exit 0
