#!/bin/sh
# Verify query encode cpu.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0069_query_encode_cpu" >/dev/null || {
    echo "not ok 1 - query encode cpu"
    exit 0
}

echo "ok 1 - query encode cpu"
exit 0
