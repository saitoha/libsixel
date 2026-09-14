#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# TGA no-background palette retains its existing black blend
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 6 || {
    echo "not ok 1 - TGA no-background palette retains its existing black blend"
    exit 0
}
echo "ok 1 - TGA no-background palette retains its existing black blend"
exit 0
