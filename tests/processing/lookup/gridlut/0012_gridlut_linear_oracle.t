#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut linear oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0012_gridlut_linear_oracle || {
    echo "not ok 1 - gridlut linear oracle"
    exit 0
}
echo "ok 1 - gridlut linear oracle"
exit 0
