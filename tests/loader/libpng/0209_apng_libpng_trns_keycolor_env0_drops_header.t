#!/bin/sh
# Verify libpng APNG output drops keycolor DCS header when SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0.

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

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_off="${ARTIFACT_LOCAL_DIR}/apng-trns-keycolor-env0-header.six"
keycolor_header="$(printf '\033P0;1q')"
out_payload=''
out_line=''
has_header=0

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - libpng APNG render failed with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
    exit 0
}

while IFS= read -r out_line || [ -n "${out_line}" ]; do
    out_payload="${out_payload}${out_line}
"
done < "${out_off}"

case "${out_payload}" in
    *"${keycolor_header}"*)
        has_header=1
        ;;
esac

if [ "${has_header}" = 1 ]; then
    echo "not ok 1 - libpng APNG kept keycolor DCS header with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
else
    echo "ok 1 - libpng APNG drops keycolor DCS header with SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
fi

exit 0
