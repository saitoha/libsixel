#!/bin/sh
# Verify builtin alpha-channel PNG keycolor is enabled by default.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png"
out_default="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-alpha-default-basn4a08.six"
out_off="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-alpha-env0-basn4a08.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_default}" || {
    echo "not ok 1 - builtin alpha default render failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              --env SIXEL_THREADS=4 \
              -Lbuiltin:cms_engine=none! \
              -d fs:scan=raster \
              "${input_png}" >"${out_off}" || {
    echo "not ok 1 - builtin alpha SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 render failed"
    exit 0
}

cmp -s "${out_default}" "${out_off}" && {
    echo "not ok 1 - builtin alpha default keycolor is unexpectedly disabled"
    exit 0
}

echo "ok 1 - builtin alpha keycolor is enabled by default"
exit 0
