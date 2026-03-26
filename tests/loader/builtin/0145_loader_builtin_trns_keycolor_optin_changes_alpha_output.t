#!/bin/sh
# Verify opt-in changes alpha-channel PNG output in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-alpha-default-basn4a08.six"
out_optin="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-alpha-optin-basn4a08.six"

run_img2sixel --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin alpha default render failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - builtin alpha opt-in render failed"
    exit 0
}

if cmp -s "${out_default}" "${out_optin}"; then
    echo "not ok 1 - opt-in unexpectedly ignored for builtin alpha PNG"
else
    echo "ok 1 - opt-in changes builtin alpha PNG output"
fi

exit 0
