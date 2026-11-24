#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# PNG RGB palette allocation failure has no callback or leak
set -eux
echo "1..1"
set -v

SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0190_png_palette_failure" || {
    printf '\n%s\n' "not ok 1 - PNG RGB palette allocation ownership"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG RGB palette allocation ownership"
exit 0
