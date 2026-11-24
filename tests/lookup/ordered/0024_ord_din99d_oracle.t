#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered din99d oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0015_ord_din99d_oracle || {
    printf '\n%s\n' "not ok 1 - ordered din99d oracle"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered din99d oracle"
exit 0
