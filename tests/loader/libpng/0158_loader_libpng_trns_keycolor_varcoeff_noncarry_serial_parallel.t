#!/bin/sh
# Verify non-carry lso2 serial keeps keycolor header.

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
output_six="${ARTIFACT_LOCAL_DIR}/trns-keycolor-lso2-serial-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=1 \
              -Llibpng:cms_engine=none! \
              -d lso2 -y raster \
              "${input_png}" >"${output_six}" || {
    echo "not ok 1 - lso2 serial render failed"
    exit 0
}

case "$(cat "${output_six}")" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - lso2 serial keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - lso2 serial lost keycolor header"
        ;;
esac

exit 0
