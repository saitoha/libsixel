#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG PREVIOUS restores the pixel under the rectangle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0118_builtin_apng_byte_previous_numeric || {
    printf '\n%s\n' "not ok 1 - byte APNG PREVIOUS"
    exit 0
}
printf '\n%s\n' "ok 1 - byte APNG PREVIOUS"
exit 0
