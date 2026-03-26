#!/bin/sh
# Verify builtin APNG output drops keycolor DCS header when SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0.

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
out_off="${ARTIFACT_LOCAL_DIR}/builtin-apng-trns-keycolor-env0-header.six"
keycolor_header="$(printf '\033P0;1q')"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 render failed"
    exit 0
}

if LC_ALL=C grep -a -q "${keycolor_header}" "${out_off}"; then
    echo "not ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 kept keycolor DCS header"
else
    echo "ok 1 - builtin APNG SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 drops keycolor DCS header"
fi

exit 0
