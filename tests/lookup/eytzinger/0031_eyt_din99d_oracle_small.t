#!/bin/sh
# Verify eytzinger din99d oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0030_eyt_din99d_oracle_small || {
    printf '\n%s\n' "not ok 1 - eytzinger din99d oracle_small"
    exit 0
}

printf '\n%s\n' "ok 1 - eytzinger din99d oracle_small"
exit 0
