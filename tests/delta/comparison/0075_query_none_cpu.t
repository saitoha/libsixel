#!/bin/sh
# Verify query none cpu.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0075_query_none_cpu" >/dev/null || {
    echo "not ok 1 - query none cpu"
    exit 0
}

echo "ok 1 - query none cpu"
exit 0
