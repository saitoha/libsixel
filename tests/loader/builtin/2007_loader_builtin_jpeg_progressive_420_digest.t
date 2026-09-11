#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG progressive 4:2:0 scans produce exact RGB.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0141_loader_builtin_jpeg_progressive_420_digest" 1>&2 || {
    echo "not ok" 1 - "JPEG progressive 4:2:0 scans produce exact RGB"
    exit 0
}

echo "ok" 1 - "JPEG progressive 4:2:0 scans produce exact RGB"

exit 0
