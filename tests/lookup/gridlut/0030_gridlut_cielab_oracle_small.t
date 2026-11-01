#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut cielab oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0024_gridlut_cielab_oracle_small || {
    echo "not ok 1 - gridlut cielab oracle small"
    exit 0
}
echo "ok 1 - gridlut cielab oracle small"
exit 0
