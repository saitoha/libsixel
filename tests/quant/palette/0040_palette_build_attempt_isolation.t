#!/bin/sh
# Verify palette build attempts are isolated and reject instance re-entry.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0040_palette_build_attempt_isolation" || {
    echo "not ok 1 - palette build attempt isolation"
    exit 0
}

echo "ok 1 - palette build attempt isolation"
exit 0
