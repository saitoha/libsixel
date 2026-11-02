#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0042_sciede2000_selection_reuse" || {
    echo "not ok 1 - sciede2000 selection reuse"
    exit 0
}
echo "ok 1 - sciede2000 selection reuse"
exit 0
