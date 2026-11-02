#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0041_sciede2000_spatial_cancellation" || {
    echo "not ok 1 - sciede2000 spatial cancellation"
    exit 0
}
echo "ok 1 - sciede2000 spatial cancellation"
exit 0
