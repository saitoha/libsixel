#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG YCCK conversion with an exact decoded RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0095_loader_builtin_jpeg_ycck8_rgb_digest" || {
    echo "not ok 1 - JPEG YCCK exact decoded RGB digest"
    exit 0
}

echo "ok 1 - JPEG YCCK exact decoded RGB digest"
exit 0
