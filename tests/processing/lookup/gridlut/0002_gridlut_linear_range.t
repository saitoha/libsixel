#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut linear range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0002_gridlut_linear_range || {
    echo "not ok 1 - gridlut linear range"
    exit 0
}
echo "ok 1 - gridlut linear range"
exit 0
