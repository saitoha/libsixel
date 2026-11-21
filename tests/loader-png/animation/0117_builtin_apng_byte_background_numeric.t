#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG BACKGROUND clears only the offset frame rectangle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0117_builtin_apng_byte_background_numeric || {
    echo "not ok 1 - byte APNG BACKGROUND"
    exit 0
}
echo "ok 1 - byte APNG BACKGROUND"
exit 0
