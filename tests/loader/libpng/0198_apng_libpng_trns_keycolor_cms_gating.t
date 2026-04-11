#!/bin/sh
# Verify cms=1 gating disables APNG keycolor behavior in libpng loader path.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
out_on="${ARTIFACT_LOCAL_DIR}/libpng-apng-trns-keycolor-cms1-on.six"
out_off="${ARTIFACT_LOCAL_DIR}/libpng-apng-trns-keycolor-cms1-off.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Llibpng:cms_engine=auto! \
              -d fs:scan=raster \
              "${input_png}" >"${out_on}" || {
    echo "not ok 1 - libpng APNG cms=1 opt-in render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms_engine=auto! \
              -d fs:scan=raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - libpng APNG cms=1 opt-out render failed"
    exit 0
}

cmp -s "${out_on}" "${out_off}" || {
    echo "not ok 1 - libpng APNG cms=1 unexpectedly keeps keycolor path"
    exit 0
}

    echo "ok 1 - libpng APNG cms=1 disables keycolor path"


exit 0
