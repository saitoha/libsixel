#!/bin/sh
# Verify P3 precision reset and reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0058_p3_precision_contract reuse || {
    echo "not ok 1 - P3 precision reset and reuse"
    exit 0
}

echo "ok 1 - P3 precision reset and reuse"
exit 0
