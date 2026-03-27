#!/bin/sh
# Verify opt-in keycolor mode changes GA16 PNG output in builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-ga16-default.six"
out_optin="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-ga16-optin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin ga16 default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -Lbuiltin:cms_engine=none! \
              -d fs -y raster \
              "${input_png}" >"${out_optin}" || {
    echo "not ok 1 - builtin ga16 opt-in render failed"
    exit 0
}

cmp -s "${out_default}" "${out_optin}" && {
    echo "not ok 1 - opt-in unexpectedly ignored for builtin ga16 PNG"
    exit 0
}

echo "ok 1 - opt-in changes builtin ga16 PNG output"

exit 0
