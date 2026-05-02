#!/bin/sh
# Exercise concurrent encoder/decoder timeline sessions sharing one writer.

set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH="${ARTIFACT_ROOT:-.}/timeline_parallel_encode_decode.jsonl" \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0002_timeline_parallel_encode_decode" || {
    echo "not ok 1 - 0002_timeline_parallel_encode_decode"
    exit 0
}

echo "ok 1 - 0002_timeline_parallel_encode_decode"
exit 0
