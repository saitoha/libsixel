#!/bin/sh
# Verify query playback cpu.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0071_query_playback_cpu" >/dev/null || {
    echo "not ok 1 - query playback cpu"
    exit 0
}

echo "ok 1 - query playback cpu"
exit 0
