#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG ICC float samples against independent JFIF and ICC calculation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0110_loader_builtin_jpeg_icc_numeric" || {
    echo "not ok 1 - JPEG embedded ICC numeric samples"
    exit 0
}

echo "ok 1 - JPEG embedded ICC numeric samples"
exit 0
