#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
# Verify gridlut linear fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/gridlut/0007_gridlut_linear_fraction || {
    printf '\n%s\n' "not ok 1 - gridlut linear fraction"
    exit 0
}
printf '\n%s\n' "ok 1 - gridlut linear fraction"
exit 0
