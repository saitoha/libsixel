#!/bin/sh
# Verify low requested colors still keep keycolor header for non-alpha tRNS PNG.

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
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-reqcolors-p1-on-tbbn0g04.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d fs:scan=raster -p 1 \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - reqcolors p1 opt-in render failed"
    exit 0
}

set +x
out_text=""
IFS= read -r out_text < "${out}" || test -n "${out_text}"
case "${out_text}" in
    *"$(printf '\033')P0;0q"*)
        echo "ok 1 - reqcolors p1 keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - reqcolors p1 lost keycolor header"
        ;;
esac

exit 0
