#!/bin/sh
# Verify compare default.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0056_compare_default" >/dev/null || {
    echo "not ok 1 - compare default"
    exit 0
}

echo "ok 1 - compare default"
exit 0
