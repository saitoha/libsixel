#!/bin/sh
# Verify query limit cpu.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0074_query_limit_cpu" >/dev/null || {
    echo "not ok 1 - query limit cpu"
    exit 0
}

echo "ok 1 - query limit cpu"
exit 0
