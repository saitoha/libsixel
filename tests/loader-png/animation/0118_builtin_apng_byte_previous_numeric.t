#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG PREVIOUS restores the pixel under the rectangle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0118_builtin_apng_byte_previous_numeric || {
    echo "not ok 1 - byte APNG PREVIOUS"
    exit 0
}
echo "ok 1 - byte APNG PREVIOUS"
exit 0
