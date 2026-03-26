#!/bin/sh
# Verify cms=auto disables keycolor behavior for RGBA16 in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png"
out_on="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-rgba16-cms-auto-on.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-rgba16-cms-auto-off.six"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin:cms_engine=auto! \
              -d fs -y raster \
              "${input_png}" >"${out_on}" || {
    echo "not ok 1 - builtin rgba16 cms=auto opt-in render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin:cms_engine=auto! \
              -d fs -y raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin rgba16 cms=auto opt-out render failed"
    exit 0
}

if cmp -s "${out_on}" "${out_off}"; then
    echo "ok 1 - builtin cms=auto disables rgba16 keycolor path"
else
    echo "not ok 1 - builtin cms=auto unexpectedly keeps rgba16 keycolor path"
fi

exit 0
