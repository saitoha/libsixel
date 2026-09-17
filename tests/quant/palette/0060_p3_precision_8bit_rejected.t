#!/bin/sh
# Verify explicit P3 byte rejection.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0059_p3_precision_contract 8bit || {
    echo "not ok 1 - explicit P3 byte rejection"
    exit 0
}

echo "ok 1 - explicit P3 byte rejection"
exit 0
