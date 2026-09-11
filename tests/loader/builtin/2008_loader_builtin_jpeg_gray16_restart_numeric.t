#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG lossless restart samples stay exact.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0142_loader_builtin_jpeg_gray16_restart_numeric" 1>&2 || {
    echo "not ok" 1 - "JPEG lossless restart samples stay exact"
    exit 0
}

echo "ok" 1 - "JPEG lossless restart samples stay exact"

exit 0
