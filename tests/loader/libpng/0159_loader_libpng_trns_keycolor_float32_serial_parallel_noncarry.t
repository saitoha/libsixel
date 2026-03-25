#!/bin/sh
# Verify float32 fs non-carry parallel keeps keycolor header.

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
output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-float32-fs-parallel-tbbn0g04.six"
run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster --precision=float32 \
              "${input_png}" >"${output_six}" || {
    echo "not ok 1 - float32 fs parallel render failed"
    exit 0
}

case "$(cat "${output_six}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - float32 fs parallel keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - float32 fs parallel lost keycolor header"
        ;;
esac

exit 0
