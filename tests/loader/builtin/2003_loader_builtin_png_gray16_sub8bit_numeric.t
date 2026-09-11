#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify PNG gray16 preserves sub-eight-bit samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0137_loader_builtin_png_gray16_sub8bit_numeric" 1>&2 || {
    echo "not ok" 1 - "PNG gray16 preserves sub-eight-bit samples"
    exit 0
}

echo "ok" 1 - "PNG gray16 preserves sub-eight-bit samples"

exit 0
