#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0039_sciede2000_uniform" || {
    echo "not ok 1 - sciede2000 uniform"
    exit 0
}
echo "ok 1 - sciede2000 uniform"
exit 0
