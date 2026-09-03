#!/bin/sh
# Pin the legacy SIXEL_LOG_LINES parsing contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0005_timeline_line_policy_environment" || {
    echo "not ok 1 - timeline line policy environment parsing changed"
    exit 0
}

echo "ok 1 - timeline line policy environment parsing is stable"
exit 0
