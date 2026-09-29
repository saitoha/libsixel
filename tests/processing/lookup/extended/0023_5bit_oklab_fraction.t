#!/bin/sh
# Verify 5bit oklab fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup fraction 1 2 || {
    echo "not ok 1 - 5bit oklab fraction"
    exit 0
}

echo "ok 1 - 5bit oklab fraction"
exit 0
