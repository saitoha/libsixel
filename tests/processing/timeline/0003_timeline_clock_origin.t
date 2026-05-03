#!/bin/sh
# Verify timeline sessions share the writer clock origin.

set -eux

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_clock_origin.jsonl"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_LOG_PATH="${log_path}" \
    "timeline/0003_timeline_clock_origin" || {
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

echo "ok 1 - 0003_timeline_clock_origin"
exit 0
