#!/bin/sh
# Verify palette+tRNS keeps keycolor header with cms=1 and explicit background.

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

input_png="${TOP_SRCDIR}/images/pngsuite/transparency/tbbn3p08.png"
out="${ARTIFACT_LOCAL_DIR}/palette-trns-cms1-white-tbbn3p08.six"

run_img2sixel -Llibpng:cms=1! \
              -B#ffffff \
              -d fs -y raster \
              "${input_png}" >"${out}" || {
    echo "not ok 1 - cms=1 background render failed"
    exit 0
}

if grep -F -q "$(printf '\033P0;1q')" "${out}"; then
    echo "ok 1 - palette+tRNS keeps keycolor header under cms=1 background"
else
    echo "not ok 1 - palette+tRNS lost keycolor header under cms=1 background"
fi

exit 0
