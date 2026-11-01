#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut gamma oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0021_gridlut_gamma_oracle_small || {
    echo "not ok 1 - gridlut gamma oracle small"
    exit 0
}
echo "ok 1 - gridlut gamma oracle small"
exit 0
