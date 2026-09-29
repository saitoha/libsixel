#!/bin/sh
# Verify rbc oklab range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup range 4 2 || {
    echo "not ok 1 - rbc oklab range"
    exit 0
}

echo "ok 1 - rbc oklab range"
exit 0
