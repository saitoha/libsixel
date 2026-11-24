#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut din99d oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0030_gridlut_din99d_oracle_mixed || {
    printf '\n%s\n' "not ok 1 - gridlut din99d oracle mixed"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut din99d oracle mixed"
exit 0
