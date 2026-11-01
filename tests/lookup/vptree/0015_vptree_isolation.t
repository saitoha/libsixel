#!/bin/sh
# Verify vptree isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup isolation 3 0 || {
    echo "not ok 1 - vptree isolation"
    exit 0
}

echo "ok 1 - vptree isolation"
exit 0
