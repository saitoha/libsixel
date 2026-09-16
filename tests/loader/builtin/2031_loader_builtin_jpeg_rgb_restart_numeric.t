#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify RGB lossless samples across component restart resets.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0165_loader_builtin_jpeg_rgb_restart_numeric" 1>&2 || {
    echo "not ok" 1 - "JPEG RGB lossless restart samples"
    exit 0
}
echo "ok" 1 - "JPEG RGB lossless restart samples"
exit 0
