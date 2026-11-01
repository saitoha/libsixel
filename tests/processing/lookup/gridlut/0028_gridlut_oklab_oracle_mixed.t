#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut oklab oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0028_gridlut_oklab_oracle_mixed || {
    echo "not ok 1 - gridlut oklab oracle mixed"
    exit 0
}
echo "ok 1 - gridlut oklab oracle mixed"
exit 0
