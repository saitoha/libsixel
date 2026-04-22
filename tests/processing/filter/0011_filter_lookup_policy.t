#!/bin/sh
# Run the prebuilt lookup-policy unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0011_filter_lookup_policy" || {
    echo "not ok 1 - 0011_filter_lookup_policy"
    exit 0
}

echo "ok 1 - 0011_filter_lookup_policy"
exit 0
