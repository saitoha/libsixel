#!/bin/sh
# Verify x_dither 8bit keeps keycolor header for a tRNS keycolor sample.

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
output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-xdither-8bit-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms_engine=none! \
              -d x_dither -y raster --precision=8bit \
              "${input_png}" >"${output_six}" || {
    echo "not ok 1 - x_dither 8bit render failed"
    exit 0
}

case "$(cat "${output_six}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - x_dither 8bit keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - x_dither 8bit lost keycolor header"
        ;;
esac

exit 0
