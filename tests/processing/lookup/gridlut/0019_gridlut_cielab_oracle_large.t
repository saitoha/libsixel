#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut cielab oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0019_gridlut_cielab_oracle_large || {
    echo "not ok 1 - gridlut cielab oracle large"
    exit 0
}
echo "ok 1 - gridlut cielab oracle large"
exit 0
