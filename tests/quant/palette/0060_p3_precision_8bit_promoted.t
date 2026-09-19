#!/bin/sh
# Verify P3 float requirement over a byte preference.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0059_p3_precision_contract 8bit || {
    echo "not ok 1 - P3 float requirement over a byte preference"
    exit 0
}

echo "ok 1 - P3 float requirement over a byte preference"
exit 0
