#!/bin/sh
# Verify process SIXEL_BGCOLOR disables keycolor path.

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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_on="${ARTIFACT_LOCAL_DIR}/trns-keycolor-bg-proc-on-tbbn0g04.six"
out_off="${ARTIFACT_LOCAL_DIR}/trns-keycolor-bg-proc-off-tbbn0g04.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_BGCOLOR=white \
              -Llibpng:cms_engine=none! -d fs -y raster \
              "${input_png}" >"${out_on}" || {
    echo "not ok 1 - process SIXEL_BGCOLOR + opt-in render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_BGCOLOR=white \
              -Llibpng:cms_engine=none! -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - process SIXEL_BGCOLOR + opt-out render failed"
    exit 0
}

cmp -s "${out_on}" "${out_off}" || {
    echo "not ok 1 - process SIXEL_BGCOLOR unexpectedly keeps keycolor path"
    exit 0
}

    echo "ok 1 - process SIXEL_BGCOLOR disables keycolor path"


exit 0
