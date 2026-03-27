#!/bin/sh
# Verify cms=1 gating disables keycolor behavior in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out_on="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-cms1-on-tbbn0g04.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-cms1-off-tbbn0g04.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin:cms_engine=auto! \
              -d fs -y raster \
              "${input_png}" >"${out_on}" || {
    echo "not ok 1 - builtin cms=1 opt-in render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin:cms_engine=auto! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin cms=1 opt-out render failed"
    exit 0
}

cmp -s "${out_on}" "${out_off}" || {
    echo "not ok 1 - builtin cms=1 unexpectedly keeps keycolor path"
    exit 0
}

echo "ok 1 - builtin cms=1 disables keycolor path"

exit 0
