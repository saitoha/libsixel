#!/bin/sh
# Verify that binning allocation failures leave no partial output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0032_filter_binning_failure_output" || {
    printf '\n%s\n' "not ok 1 - binning failure output is atomic"
    exit 0
}

printf '\n%s\n' "ok 1 - binning failure output is atomic"
exit 0
