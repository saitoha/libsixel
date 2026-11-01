#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut oklab oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0013_gridlut_oklab_oracle || {
    echo "not ok 1 - gridlut oklab oracle"
    exit 0
}
echo "ok 1 - gridlut oklab oracle"
exit 0
