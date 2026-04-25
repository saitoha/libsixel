#!/bin/sh
# Run the prebuilt dither-policy unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/0001_dither_policy" || {
    echo "not ok 1 - 0001_dither_policy"
    exit 0
}

echo "ok 1 - 0001_dither_policy"
exit 0
