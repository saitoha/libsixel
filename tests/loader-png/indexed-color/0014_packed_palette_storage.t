#!/bin/sh
# Libpng reports the allocated packed pixel and palette bytes.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo '1..0 # SKIP libpng unavailable'
    exit 0
}
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0022_loader_libpng_indexed_png_reqcolors_fallback packed-storage || {
    echo 'not ok 1 - Libpng reports the allocated packed pixel and palette bytes'
    exit 0
}
echo 'ok 1 - Libpng reports the allocated packed pixel and palette bytes'
exit 0
