#!/bin/sh
# Policy: docs/encoder/precision/README.md
# Verify explicit float preserve on the actual working frame.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    precision/0008_explicit_float_preserve 1>&2 || {
    echo "not ok 1 - explicit float preserve"
    exit 0
}
echo "ok 1 - explicit float preserve"
exit 0
