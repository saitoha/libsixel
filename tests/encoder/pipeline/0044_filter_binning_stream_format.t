#!/bin/sh
# Verify non-canonical sample-stream pixel layouts are rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0035_filter_binning_stream_format" || {
    echo "not ok 1 - sample-stream format contract"
    exit 0
}

echo "ok 1 - sample-stream format contract"
exit 0
