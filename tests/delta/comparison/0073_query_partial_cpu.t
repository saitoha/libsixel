#!/bin/sh
# Verify query partial cpu.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0073_query_partial_cpu" >/dev/null || {
    echo "not ok 1 - query partial cpu"
    exit 0
}

echo "ok 1 - query partial cpu"
exit 0
