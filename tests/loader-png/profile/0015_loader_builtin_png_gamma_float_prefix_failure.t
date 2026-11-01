#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Preserve every source component after a qualified gamma float prefix failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0193_png_metadata_failures 9 || {
    echo "not ok 1 - PNG gamma float prefix failure"
    exit 0
}
echo "ok 1 - PNG gamma float prefix failure"
exit 0
