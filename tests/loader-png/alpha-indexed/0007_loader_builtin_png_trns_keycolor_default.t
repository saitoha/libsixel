#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify the PNG tRNS keycolor default directly, without a pixel difference.
set -eux
echo "1..1"
set -v

# An empty override resolves the registered default. Both palette routes now
# preserve source color and coverage, so output differences cannot prove it.
SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 8 || {
    printf '\n%s\n' "not ok 1 - PNG tRNS keycolor default is enabled"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG tRNS keycolor default is enabled"
exit 0
