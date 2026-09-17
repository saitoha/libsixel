#!/bin/sh
# Verify automatic P3 precision.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0059_p3_precision_contract auto || {
    echo "not ok 1 - automatic P3 precision"
    exit 0
}

echo "ok 1 - automatic P3 precision"
exit 0
