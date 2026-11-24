#!/bin/sh
# Verify certlut gamma fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup fraction 0 0 || {
    printf '\n%s\n' "not ok 1 - certlut gamma fraction"
    exit 0
}

printf '\n%s\n' "ok 1 - certlut gamma fraction"
exit 0
