#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# PNG all zero-alpha palette and source RGB, keycolor 0
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 3 || {
    echo "not ok 1 - PNG all zero-alpha palette and source RGB, keycolor 0"
    exit 0
}
echo "ok 1 - PNG all zero-alpha palette and source RGB, keycolor 0"
exit 0
