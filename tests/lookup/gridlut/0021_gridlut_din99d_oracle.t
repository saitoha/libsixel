#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut din99d oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0015_gridlut_din99d_oracle || {
    echo "not ok 1 - gridlut din99d oracle"
    exit 0
}
echo "ok 1 - gridlut din99d oracle"
exit 0
