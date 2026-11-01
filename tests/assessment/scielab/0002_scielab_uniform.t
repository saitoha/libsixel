#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0027_scielab_uniform" || {
    echo "not ok 1 - scielab uniform"
    exit 0
}
echo "ok 1 - scielab uniform"
exit 0
