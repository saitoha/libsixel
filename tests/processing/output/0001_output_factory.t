#!/bin/sh
# Run the prebuilt output factory unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "output/0001_output_factory" || {
    echo "not ok 1 - 0001_output_factory"
    exit 0
}

echo "ok 1 - 0001_output_factory"
exit 0
