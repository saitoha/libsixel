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
    printf '\n%s\n' 'not ok 1 - Libpng reports the allocated packed pixel and palette bytes'
    exit 0
}
printf '\n%s\n' 'ok 1 - Libpng reports the allocated packed pixel and palette bytes'
exit 0
