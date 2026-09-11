#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify APNG PREVIOUS disposal with exact decoded-frame pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0094_loader_builtin_png_apng_dispose_previous_numeric" || {
    echo "not ok 1 - APNG PREVIOUS disposal decoded-frame pixels"
    exit 0
}

echo "ok 1 - APNG PREVIOUS disposal decoded-frame pixels"
exit 0
