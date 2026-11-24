#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered din99d oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0020_ord_din99d_oracle_large || {
    printf '\n%s\n' "not ok 1 - ordered din99d oracle large"
    exit 0
}
printf '\n%s\n' "ok 1 - ordered din99d oracle large"
exit 0
