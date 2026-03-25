#!/bin/sh
# Verify repeated short -% assignments honor the last value.

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-repeated-short-last0-tbbn0g04.six"
run_img2sixel -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              -% SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
              -Llibpng:cms=0! -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - repeated short -% render failed"
    exit 0
}

case "$(cat "${out}")" in
    *"$(printf '\033')P0;1q"*)
        echo "not ok 1 - repeated short -% did not use last value"
        ;;
    *)
        echo "ok 1 - repeated short -% uses last value"
        ;;
esac

exit 0
