#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify the PNG tRNS keycolor default directly, without a pixel difference.
set -eux
echo "1..1"
set -v

# An empty override resolves the registered default. Both palette routes now
# preserve source color and coverage, so output differences cannot prove it.
SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 8 || {
    echo "not ok 1 - PNG tRNS keycolor default is enabled"
    exit 0
}
echo "ok 1 - PNG tRNS keycolor default is enabled"
exit 0
