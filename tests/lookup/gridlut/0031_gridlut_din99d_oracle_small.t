#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut din99d oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0025_gridlut_din99d_oracle_small || {
    printf '\n%s\n' "not ok 1 - gridlut din99d oracle small"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut din99d oracle small"
exit 0
