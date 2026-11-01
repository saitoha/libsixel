#!/bin/sh
# Verify rbc linear oracle_large.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup oracle_large 4 1 || {
    echo "not ok 1 - rbc linear oracle_large"
    exit 0
}

echo "ok 1 - rbc linear oracle_large"
exit 0
