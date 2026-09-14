#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# PNG explicit background retains the existing byte blend
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 5 || {
    echo "not ok 1 - PNG explicit background retains the existing byte blend"
    exit 0
}
echo "ok 1 - PNG explicit background retains the existing byte blend"
exit 0
