#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify region spatial cancellation.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0006_region_spatial_cancellation" || {
    echo "not ok 1 - region spatial cancellation"
    exit 0
}
echo "ok 1 - region spatial cancellation"
exit 0
