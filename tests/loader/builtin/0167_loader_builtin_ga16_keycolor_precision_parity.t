#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify GA16 keycolor selection preserves the high-depth PNG path.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 --env SIXEL_THREADS=4 \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    "${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png") || {
    echo "not ok 1 - first ga16 render failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 --env SIXEL_THREADS=4 \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    "${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png") || {
    echo "not ok 1 - second ga16 render failed"
    exit 0
}
test "$first" = "$second" || {
    echo "not ok 1 - keycolor selection changed original ga16 precision"
    exit 0
}
echo "ok 1 - keycolor selection preserves ga16 precision"
exit 0
