#!/bin/sh
# Verify invalid background colorspace falls back to gamma in libpng path.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png"
out_invalid="${ARTIFACT_LOCAL_DIR}/libpng_bgcs_invalid.six"
out_gamma="${ARTIFACT_LOCAL_DIR}/libpng_bgcs_gamma_explicit.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=invalid \
              -Llibpng:cms_engine=none! \
              -B#808080 "${input_png}" >"${out_invalid}" || {
    echo "not ok 1 - libpng invalid background colorspace render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibpng:cms_engine=none! \
              -B#808080 "${input_png}" >"${out_gamma}" || {
    echo "not ok 1 - libpng gamma background colorspace render failed"
    exit 0
}

cmp -s "${out_invalid}" "${out_gamma}" || {
    echo "not ok 1 - libpng invalid background colorspace changed output"
    exit 0
}

    echo "ok 1 - libpng invalid background colorspace falls back to gamma"


exit 0
