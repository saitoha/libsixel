#!/bin/sh
# Verify short -% env override enables keycolor over process opt-out.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/builtin-trns-keycolor-cli-short-override-tbbn0g04.six"

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
                  -Lbuiltin:cms_engine=none! -d fs -y raster \
                  "${input_png}" >"${out}" || {
    echo "not ok 1 - builtin process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 + -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 render failed"
    exit 0
}

set +x
out_text=""
IFS= read -r out_text < "${out}" || test -n "${out_text}"
case "${out_text}" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - builtin -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 overrides process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
        ;;
    *)
        echo "not ok 1 - builtin -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 did not override process SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0"
        ;;
esac

exit 0
