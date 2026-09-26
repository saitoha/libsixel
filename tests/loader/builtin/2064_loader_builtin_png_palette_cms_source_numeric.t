#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# PNG palette CMS consumes original source RGB
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0189_png_palette_samples" 4 || {
    echo "not ok 1 - PNG palette CMS consumes original source RGB"
    exit 0
}
echo "ok 1 - PNG palette CMS consumes original source RGB"
exit 0
