#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Check that byte APNG OVER replaces a pixel with an opaque source.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader-png/animation/0120_builtin_apng_byte_over_opaque_numeric || {
    echo "not ok 1 - byte APNG opaque OVER"
    exit 0
}
echo "ok 1 - byte APNG opaque OVER"
exit 0
