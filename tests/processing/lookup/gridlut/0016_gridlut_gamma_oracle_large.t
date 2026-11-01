#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut gamma oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0016_gridlut_gamma_oracle_large || {
    echo "not ok 1 - gridlut gamma oracle large"
    exit 0
}
echo "ok 1 - gridlut gamma oracle large"
exit 0
