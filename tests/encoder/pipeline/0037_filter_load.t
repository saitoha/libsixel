#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0008_filter_load" || {
    echo "not ok 1 - 0008_filter_load"
    exit 0
}

echo "ok 1 - 0008_filter_load"
exit 0
