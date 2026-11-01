#!/bin/sh
# Verify step source refresh.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0087_step_source_refresh" || {
    echo "not ok 1 - step source refresh"
    exit 0
}
echo "ok 1 - step source refresh"
exit 0
