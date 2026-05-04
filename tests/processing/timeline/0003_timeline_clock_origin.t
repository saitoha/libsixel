#!/bin/sh
# Verify timeline sessions share the writer clock origin.

set -eux

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_clock_origin.jsonl"

MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_LOG_PATH="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0003_timeline_clock_origin" || {
    echo "not ok 1 - 0003_timeline_clock_origin"
    exit 0
}

MSYS_NO_PATHCONV=1 \
MSYS2_ARG_CONV_EXCL='*' \
SIXEL_TEST_TIMELINE_JSONL="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0003_timeline_clock_origin_verify" || {
    echo "not ok 1 - 0003_timeline_clock_origin_verify"
    exit 0
}

echo "ok 1 - 0003_timeline_clock_origin"
exit 0
