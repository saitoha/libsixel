#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify all lossless predictors together while paying one process startup.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0164_loader_builtin_jpeg_lossless_predictors_numeric" 1>&2 || {
    echo "not ok" 1 - "JPEG lossless predictors and point transform"
    exit 0
}
echo "ok" 1 - "JPEG lossless predictors and point transform"
exit 0
