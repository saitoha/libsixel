#!/bin/sh
# Verify APNG honors strict keycolor env parsing: value 2 matches explicit opt-out in builtin loader.

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
out_invalid="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env2.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env0.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
              -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_invalid}" || {
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 render failed"
    exit 0
}

if cmp -s "${out_invalid}" "${out_off}"; then
    echo "ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 is ignored and stays opt-out"
else
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 unexpectedly changes output"
fi

exit 0
