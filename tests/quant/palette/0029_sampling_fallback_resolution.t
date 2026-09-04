#!/bin/sh
# Verify the sampling fallback resolution contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0029_sampling_fallback_resolution" || {
    echo "not ok 1 - sampling fallback resolution"
    exit 0
}

echo "ok 1 - sampling fallback resolution"
exit 0
