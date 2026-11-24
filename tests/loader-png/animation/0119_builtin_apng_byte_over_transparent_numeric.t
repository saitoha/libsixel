#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG OVER clears hidden RGB at zero combined coverage.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0119_builtin_apng_byte_over_transparent_numeric || {
    printf '\n%s\n' "not ok 1 - byte APNG transparent OVER"
    exit 0
}
printf '\n%s\n' "ok 1 - byte APNG transparent OVER"
exit 0
