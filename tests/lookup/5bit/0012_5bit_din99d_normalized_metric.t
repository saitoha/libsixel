#!/bin/sh
# Verify 5bit din99d normalized metric.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/extended/0001_extended_lookup metric 1 4 || {
    printf '\n%s\n' "not ok 1 - 5bit din99d normalized metric"
    exit 0
}

printf '\n%s\n' "ok 1 - 5bit din99d normalized metric"
exit 0
