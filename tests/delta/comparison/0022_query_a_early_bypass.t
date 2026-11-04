#!/bin/sh
# Verify query a early bypass.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0022_query_a_early_bypass" >/dev/null || {
    echo "not ok 1 - query a early bypass"
    exit 0
}

echo "ok 1 - query a early bypass"
exit 0
