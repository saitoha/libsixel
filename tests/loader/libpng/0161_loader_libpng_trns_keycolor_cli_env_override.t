#!/bin/sh
# Verify long --env overrides process env for keycolor opt-in.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-cli-long-override-tbbn0g04.six"

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Llibpng:cms_engine=none! -d fs -y raster \
                  "${input_png}" >"${out}" || {
    echo "not ok 1 - process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 + --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 render failed"
    exit 0
}

case "$(cat "${out}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 overrides process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
        ;;
    *)
        echo "not ok 1 - --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 did not override process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
        ;;
esac

exit 0
