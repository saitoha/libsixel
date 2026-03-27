#!/bin/sh
# Verify empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR matches default behavior in builtin APNG path.

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
out_empty="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env-empty.six"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-default.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR= \
              -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_empty}" || {
    echo "not ok 1 - builtin APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR render failed"
    exit 0
}

run_img2sixel -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin APNG default render failed"
    exit 0
}

cmp -s "${out_empty}" "${out_default}" || {
    echo "not ok 1 - builtin APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR unexpectedly differs from default"
    exit 0
}

echo "ok 1 - builtin APNG empty SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR matches default"
exit 0
