#!/bin/sh
# Verify precision-policy=8bit rejects a Display P3 destination.
# Policy: docs/encoder/output/output-colorspace.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0059_p3_precision_contract 8bit || {
    echo "not ok 1 - 8bit precision policy accepted a P3 destination"
    exit 0
}

echo "ok 1 - 8bit precision policy rejects a P3 destination"
exit 0
