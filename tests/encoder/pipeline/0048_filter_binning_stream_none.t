#!/bin/sh
# Verify none binning preserves visible sample-stream points in scan order.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0039_filter_binning_stream_none" || {
    echo "not ok 1 - none sample-stream binning contract"
    exit 0
}

echo "ok 1 - none sample-stream binning contract"
exit 0
