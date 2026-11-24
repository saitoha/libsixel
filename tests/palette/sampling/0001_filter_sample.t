#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0002_filter_sample" || {
    printf '\n%s\n' "not ok 1 - 0002_filter_sample"
    exit 0
}

printf '\n%s\n' "ok 1 - 0002_filter_sample"
exit 0
