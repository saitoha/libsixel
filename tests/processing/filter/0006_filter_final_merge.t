#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0006_filter_final_merge" || {
    echo "not ok 1 - 0006_filter_final_merge"
    exit 0
}

echo "ok 1 - 0006_filter_final_merge"
exit 0
