#!/bin/sh
# Verify lookup policy=bogus keeps keycolor header.

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
out="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lookup-bogus-on-tbbn0g04.six"
run_img2sixel --env SIXEL_DITHER_LOOKUP_POLICY=bogus \
              --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=4 \
              -Llibpng:cms=0! \
              -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - lookup policy=bogus opt-in render failed"
    exit 0
}

case "$(cat "${out}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - lookup policy=bogus keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - lookup policy=bogus lost keycolor header"
        ;;
esac

exit 0
