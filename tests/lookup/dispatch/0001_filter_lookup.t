#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0005_filter_lookup" || {
    printf '\n%s\n' "not ok 1 - 0005_filter_lookup"
    exit 0
}

printf '\n%s\n' "ok 1 - 0005_filter_lookup"
exit 0
