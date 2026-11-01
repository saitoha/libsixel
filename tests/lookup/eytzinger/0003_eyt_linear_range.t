#!/bin/sh
# Verify eytzinger linear range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0002_eyt_linear_range || {
    echo "not ok 1 - eytzinger linear range"
    exit 0
}

echo "ok 1 - eytzinger linear range"
exit 0
