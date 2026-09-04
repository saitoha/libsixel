#!/bin/sh
# Verify that binning allocation failures leave no partial output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0032_filter_binning_failure_output" || {
    echo "not ok 1 - binning failure output is atomic"
    exit 0
}

echo "ok 1 - binning failure output is atomic"
exit 0
