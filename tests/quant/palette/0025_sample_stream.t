#!/bin/sh
# Verify explicit ownership and metadata in the sample-stream artifact.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0025_sample_stream" || {
    echo "not ok 1 - sample stream ownership contract"
    exit 0
}

echo "ok 1 - sample stream ownership contract"
exit 0
