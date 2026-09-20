#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Resolve delayed height with explicit float32 output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0204_jpeg_dnl_float_numeric" 1>&2 || {
    echo "not ok 1 - Resolve delayed height with explicit float32 output"
    exit 0
}
echo "ok 1 - Resolve delayed height with explicit float32 output"
exit 0
