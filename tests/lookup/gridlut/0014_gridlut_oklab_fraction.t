#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut oklab fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0008_gridlut_oklab_fraction || {
    echo "not ok 1 - gridlut oklab fraction"
    exit 0
}
echo "ok 1 - gridlut oklab fraction"
exit 0
