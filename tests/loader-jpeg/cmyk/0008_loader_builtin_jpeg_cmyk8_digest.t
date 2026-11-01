#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG CMYK conversion stays exact.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0144_loader_builtin_jpeg_cmyk8_digest" 1>&2 || {
    echo "not ok" 1 - "JPEG CMYK conversion stays exact"
    exit 0
}

echo "ok" 1 - "JPEG CMYK conversion stays exact"

exit 0
