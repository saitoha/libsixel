#!/bin/sh
# Verify 5bit cielab fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup fraction 1 3 || {
    echo "not ok 1 - 5bit cielab fraction"
    exit 0
}

echo "ok 1 - 5bit cielab fraction"
exit 0
