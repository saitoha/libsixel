#!/bin/sh
# Exercise concurrent encoder/decoder timeline sessions sharing one writer.

set -eux

echo "1..1"
set -v

log_path="${ARTIFACT_ROOT:-.}/timeline_parallel_encode_decode.jsonl"

SIXEL_LOG_PATH="${log_path}" \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0002_timeline_parallel_encode_decode" || {
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

echo "ok 1 - 0002_timeline_parallel_encode_decode"
exit 0
