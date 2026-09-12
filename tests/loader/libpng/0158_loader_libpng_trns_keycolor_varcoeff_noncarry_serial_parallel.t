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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn0g04.png"
output_six="${TMPDIR:-/tmp}/libsixel-${0##*/}-$$-trns-keycolor-lso2-serial-tbbn0g04.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Akeep --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
              --env SIXEL_THREADS=1 \
              -Llibpng:cms_engine=none! \
              -d lso2:scan=raster \
              "${input_png}" >"${output_six}" || {
    echo "not ok 1 - lso2 serial render failed"
    exit 0
}

set +x
output_six_text=""
IFS= read -r output_six_text < "${output_six}" || test -n "${output_six_text}"
case "${output_six_text}" in
    *"$(printf '\033')P0;1q"*)
        echo "ok 1 - lso2 serial keeps keycolor header"
        ;;
    *)
        echo "not ok 1 - lso2 serial lost keycolor header"
        ;;
esac

exit 0
