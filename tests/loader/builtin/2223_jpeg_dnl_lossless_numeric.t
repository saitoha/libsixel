#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Preserve lossless predictor samples after delayed height.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0207_jpeg_dnl_lossless_numeric" 1>&2 || {
    echo "not ok 1 - Preserve lossless predictor samples after delayed height"
    exit 0
}
echo "ok 1 - Preserve lossless predictor samples after delayed height"
exit 0
