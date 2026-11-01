#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut linear oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0022_gridlut_linear_oracle_small || {
    echo "not ok 1 - gridlut linear oracle small"
    exit 0
}
echo "ok 1 - gridlut linear oracle small"
exit 0
