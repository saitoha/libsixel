#!/bin/sh
# Verify eytzinger linear fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0007_eyt_linear_fraction || {
    echo "not ok 1 - eytzinger linear fraction"
    exit 0
}

echo "ok 1 - eytzinger linear fraction"
exit 0
