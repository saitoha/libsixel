#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# PNG none zero-alpha palette and source RGB, keycolor 0
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 2 || {
    printf '\n%s\n' "not ok 1 - PNG none zero-alpha palette and source RGB, keycolor 0"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG none zero-alpha palette and source RGB, keycolor 0"
exit 0
