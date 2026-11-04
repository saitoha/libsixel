#!/bin/sh
# Verify query x early bypass.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0031_query_x_early_bypass" >/dev/null || {
    echo "not ok 1 - query x early bypass"
    exit 0
}

echo "ok 1 - query x early bypass"
exit 0
