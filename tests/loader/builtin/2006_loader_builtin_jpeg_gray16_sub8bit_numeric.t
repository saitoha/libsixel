#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify JPEG gray16 preserves sub-eight-bit samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0140_loader_builtin_jpeg_gray16_sub8bit_numeric" 1>&2 || {
    echo "not ok" 1 - "JPEG gray16 preserves sub-eight-bit samples"
    exit 0
}

echo "ok" 1 - "JPEG gray16 preserves sub-eight-bit samples"

exit 0
