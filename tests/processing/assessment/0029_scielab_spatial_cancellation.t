#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0029_scielab_spatial_cancellation" || {
    echo "not ok 1 - scielab spatial cancellation"
    exit 0
}
echo "ok 1 - scielab spatial cancellation"
exit 0
