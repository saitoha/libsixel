#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify region partial edge.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0007_region_partial_edge" || {
    printf '\n%s\n' "not ok 1 - region partial edge"
    exit 0
}
printf '\n%s\n' "ok 1 - region partial edge"
exit 0
