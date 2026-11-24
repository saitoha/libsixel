#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG BACKGROUND clears only the offset frame rectangle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0117_builtin_apng_byte_background_numeric || {
    printf '\n%s\n' "not ok 1 - byte APNG BACKGROUND"
    exit 0
}
printf '\n%s\n' "ok 1 - byte APNG BACKGROUND"
exit 0
