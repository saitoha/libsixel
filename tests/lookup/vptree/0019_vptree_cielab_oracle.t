#!/bin/sh
# Verify vptree cielab oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup oracle 3 3 || {
    echo "not ok 1 - vptree cielab oracle"
    exit 0
}

echo "ok 1 - vptree cielab oracle"
exit 0
