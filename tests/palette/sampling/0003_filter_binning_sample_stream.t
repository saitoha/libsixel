#!/bin/sh
# Verify the binning filter consumes a sample stream without a point copy.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0034_filter_binning_sample_stream" || {
    echo "not ok 1 - sample-stream binning filter contract"
    exit 0
}

echo "ok 1 - sample-stream binning filter contract"
exit 0
