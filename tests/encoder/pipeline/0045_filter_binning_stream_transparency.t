#!/bin/sh
# Verify sample-stream binning excludes explicitly masked samples.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0036_filter_binning_stream_transparency" || {
    echo "not ok 1 - sample-stream transparent-mask contract"
    exit 0
}

echo "ok 1 - sample-stream transparent-mask contract"
exit 0
