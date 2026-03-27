#!/bin/sh
# Verify linear background interpretation changes indexed+tRNS composition.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tm3n3p02.png"
output_gamma="${ARTIFACT_LOCAL_DIR}/libpng_bgcs_indexed_gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/libpng_bgcs_indexed_linear.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibpng:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_gamma}" || {
    echo "not ok" 1 - "libpng indexed gamma background composition failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Llibpng:cms_engine=none! \
              -B#808080 "${input_png}" >"${output_linear}" || {
    echo "not ok" 1 - "libpng indexed linear background composition failed"
    exit 0
}

cmp -s "${output_gamma}" "${output_linear}" && {
    echo "not ok" 1 - "gamma and linear indexed composition produced identical output"
    exit 0
}


echo "ok" 1 - "linear background interpretation changes indexed+tRNS composition"
exit 0
