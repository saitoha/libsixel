#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut gamma oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0011_gridlut_gamma_oracle || {
    echo "not ok 1 - gridlut gamma oracle"
    exit 0
}
echo "ok 1 - gridlut gamma oracle"
exit 0
