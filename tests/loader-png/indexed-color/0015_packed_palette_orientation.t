#!/bin/sh
# Libpng reflects packed palette indices in display order.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo '1..0 # SKIP libpng unavailable'
    exit 0
}
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0022_loader_libpng_indexed_png_reqcolors_fallback mirror || {
    echo 'not ok 1 - Libpng reflects packed palette indices in display order'
    exit 0
}
echo 'ok 1 - Libpng reflects packed palette indices in display order'
exit 0
