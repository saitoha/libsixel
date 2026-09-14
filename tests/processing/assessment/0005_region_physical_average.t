#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify region physical average.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0005_region_physical_average" || {
    echo "not ok 1 - region physical average"
    exit 0
}
echo "ok 1 - region physical average"
exit 0
