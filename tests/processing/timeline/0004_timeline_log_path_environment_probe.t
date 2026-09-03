#!/bin/sh
# Pin the one-shot SIXEL_LOG_PATH environment probe contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0004_timeline_log_path_environment_probe" || {
    echo "not ok 1 - timeline log path environment probe is one-shot"
    exit 0
}

echo "ok 1 - timeline log path environment probe is one-shot"
exit 0
