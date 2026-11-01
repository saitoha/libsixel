#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0032_scielab_physical_signal" || {
    echo "not ok 1 - scielab physical signal"
    exit 0
}
echo "ok 1 - scielab physical signal"
exit 0
